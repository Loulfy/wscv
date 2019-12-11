#include <iostream>
#include <mongoose/mongoose.h>
#include <opencv2/opencv.hpp>

#include <wscv.pb.h>

using namespace std;

static sig_atomic_t s_signal_received = 0;
static const char *s_http_port = "9090";
static struct mg_serve_http_opts s_http_server_opts;

static bool show = true;

static void signal_handler(int sig_num)
{
    signal(sig_num, signal_handler);  // Reinstantiate signal handler
    s_signal_received = sig_num;
}

static const cv::Scalar skin_lower(0, 48, 80);
static const cv::Scalar skin_upper(20, 255, 255);

bool contourSort(vector<cv::Point>& a, vector<cv::Point>& b) { return cv::contourArea(a) > contourArea(b); }

static void compute(cv::Mat& frame, wscv::Response& res)
{
    cv::flip(frame, frame, 0);
    cv::flip(frame, frame, 1);

    cv::cvtColor(frame, frame, cv::COLOR_RGB2HSV);
    cv::inRange(frame, skin_lower, skin_upper, frame);
    cv::erode(frame, frame, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(6, 6)), cv::Point(-1, -1), 2);
    cv::dilate(frame, frame, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(6, 6)), cv::Point(-1, -1), 2);

    vector<vector<cv::Point>> contours;
    cv::findContours(frame, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
    if(!contours.empty())
    {
        cv::Point2f center;
        float radius;
        std::sort(contours.begin(), contours.end(), contourSort);
        minEnclosingCircle(contours.front(), center, radius);
        res.set_status("success");
        res.set_trigger(false);
        res.set_x(center.x);
        res.set_y(center.y);

        //drawContours(frame, contours, -1, cv::Scalar(50, 255, 255));
        //circle(frame, center, radius, cv::Scalar(255, 0, 0));
    }
    else res.set_status("failure");

    //if(show) cv::imshow("camera", frame);
    //if((cv::waitKey(30) & 0xff) == 27) show = false;
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
    switch (ev)
    {
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
        {
            cout << "Bonjour Unity" << endl;
            break;
        }
        case MG_EV_WEBSOCKET_FRAME:
        {
            struct websocket_message *wm = (struct websocket_message *) ev_data;

            if(wm->flags & WEBSOCKET_OP_BINARY)
            {
                wscv::Request req;

                req.ParseFromArray(wm->data, wm->size);
                cout << "Protobuf frame : " << req.width() << "x" << req.height() << " = " << req.content().size() << " bytes" << endl;

                cv::Mat frame(req.height(), req.width(), CV_8UC3, (unsigned char*)req.content().data());
                wscv::Response res;

                compute(frame, res);

                auto msg = res.SerializeAsString();

                mg_send_websocket_frame(nc, WEBSOCKET_OP_BINARY, msg.c_str(), msg.size());
            }

            break;
        }
        case MG_EV_HTTP_REQUEST:
        {
            mg_serve_http(nc, (struct http_message *) ev_data, s_http_server_opts);
            break;
        }
        case MG_EV_CLOSE:
        {
            break;
        }
    }
}

int main()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    struct mg_mgr mgr;
    struct mg_connection *nc;

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    mg_mgr_init(&mgr, NULL);

    nc = mg_bind(&mgr, s_http_port, ev_handler);
    mg_set_protocol_http_websocket(nc);
    s_http_server_opts.document_root = WWW_DIR;  // Serve current directory
    s_http_server_opts.enable_directory_listing = "yes";

    printf("Started on port %s\n", s_http_port);
    while (s_signal_received == 0)
    {
        mg_mgr_poll(&mgr, 200);
    }
    mg_mgr_free(&mgr);
    google::protobuf::ShutdownProtobufLibrary();

    return EXIT_SUCCESS;
}
