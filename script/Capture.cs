using System;
using UnityEngine;
using System.Net.WebSockets;
using System.Threading;
using Google.Protobuf;
using Wscv;

public class Capture : MonoBehaviour
{
    private WebCamTexture cam;
    private ClientWebSocket ws;
    private Texture2D frame;
    private byte[] buffer;
    
    // Start is called before the first frame update
    void Start()
    {
        // Create video capture device
        cam = new WebCamTexture();
        Debug.Log(cam.deviceName);
        var renderer = GetComponent<Renderer>();
        renderer.material.mainTexture = cam;
        cam.Play();
        
        // Connect
        ws = new ClientWebSocket();
        ws.ConnectAsync(new Uri("ws://lylix.io:9090"), CancellationToken.None).Wait();

        // Create buffer frame
        Debug.Log(cam.width+ "x" +cam.height);
        frame = new Texture2D(cam.width, cam.height, TextureFormat.RGB24, false);

        // Buffer for response
        buffer = new byte[1024];

        //await ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "bye", CancellationToken.None);
    }

    // Update is called once per frame
    void Update()
    {
        // Capture Frame
        frame.SetPixels(cam.GetPixels());

        // Prepare request
        var req = new Request
        {
            Width = frame.width,
            Height = frame.height,
            Method = Request.Types.Method.Set,
            Content = ByteString.CopyFrom(frame.GetRawTextureData())
        };

        // Send Request
        var segment = new ArraySegment<byte>(req.ToByteArray());
        ws.SendAsync(segment, WebSocketMessageType.Binary, true, CancellationToken.None).Wait();

        // Get Response
        WebSocketReceiveResult r;
        segment = new ArraySegment<byte>(buffer);
        do
        {
            r = ws.ReceiveAsync(segment, CancellationToken.None).Result;
        } while (!r.EndOfMessage);

        // TODO : do something with response
        var res = Response.Parser.ParseFrom(ByteString.CopyFrom(buffer, segment.Offset, r.Count));
        Debug.Log(res.Status);
        Camera.main.backgroundColor = res.Status == "success" ? Color.green : Color.red;
    }
}
