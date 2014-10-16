using System.Net.Sockets;
using System.Net;
using System;
using System.Text.RegularExpressions;
using System.Text;
using System.Security.Cryptography;
using System.Collections.Generic;


class Server {
	static byte[] GetBytes(string str)
	{
	    byte[] bytes = new byte[str.Length];

	    for (int i = 0; i < str.Length; i++) {
	    	bytes[i] = (Byte)str[i];
	    }
	    return bytes;
	}

	public static void printByteArray(Byte[] array) {
		foreach (Byte b in array) {
					Console.Write(string.Format("{0} ", (uint)b));
				}

		Console.WriteLine();
	}

	public static void printByteArray(String array) {
		foreach (char b in array) {
					Console.Write(string.Format("{0} ", b));

				}
		Console.WriteLine();

	}
	public static void Main(string[] args) {

		Console.WriteLine("HTTP/1.1 101 Switching Protocols" + "\r\n"
			        + "Connection: Upgrade" + "\r\n"
			        + "Upgrade: websocket" + "\r\n"
			        + "Sec-WebSocket-Accept: " + Convert.ToBase64String (
			            SHA1.Create().ComputeHash (
			                Encoding.UTF8.GetBytes (
			                    args[0].Trim() 
			                    + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))));
		return ;
	
		TcpListener server = new TcpListener(IPAddress.Parse("127.0.0.1"), 8181);

		server.Start();
		Console.WriteLine("Server has started on 127.0.0.1:8181.{0}Waiting for a connection...", "\r\n");

		TcpClient client = server.AcceptTcpClient();

		Console.WriteLine("A client connected.");

		NetworkStream stream = client.GetStream();

		//enter to an infinite cycle to be able to handle every change in stream
		while (true) {
			while (!stream.DataAvailable);

			Byte[] bytes = new Byte[client.Available];

			stream.Read(bytes, 0, bytes.Length);

			//translate bytes of request to string
			String data = Encoding.UTF8.GetString(bytes);

			if (new Regex("^GET").IsMatch(data)) {
				Byte[] response = Encoding.UTF8.GetBytes("HTTP/1.1 101 Switching Protocols" + "\r\n"
			        + "Connection: Upgrade" + "\r\n"
			        + "Upgrade: websocket" + "\r\n"
			        + "Sec-WebSocket-Accept: " + Convert.ToBase64String (
			            SHA1.Create().ComputeHash (
			                Encoding.UTF8.GetBytes (
			                    new Regex("Sec-WebSocket-Key: (.*)").Match(data).Groups[1].Value.Trim() 
			                    + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
			                )
			            )
			        ) + "\r\n"
			        + "\r\n");

				Console.WriteLine(Encoding.UTF8.GetString(response));
			    stream.Write(response, 0, response.Length);

			} else {
				const byte DATA_START = 6;
				const byte KEY_START = 2;
				const byte HEADER_SIZE = 2;
				const byte KEY_SIZE = 4;

				Byte[] byte_data = GetBytes(data);
				Byte[] header = new Byte[HEADER_SIZE];
				Byte[] key = new Byte[KEY_SIZE];
				Byte[] encoded = new Byte[byte_data.Length - DATA_START];
				Byte[] decoded = new Byte[byte_data.Length - DATA_START];

				Array.Copy (bytes, 0, header, 0, 2);
				Array.Copy (bytes, KEY_START, key, 0, 4);
				Array.Copy (bytes, DATA_START, encoded, 0, encoded.Length);


				for (int i = 0; i < encoded.Length; i++ ) {
					decoded[i] = (Byte)(encoded[i] ^ key[i % 4]);
				}

				Console.WriteLine();
				Console.WriteLine(Encoding.ASCII.GetString(decoded));

				List<byte> lb= new List<byte>();
				// see http://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-17 
				//     page 30 for this:
				// 0                   1                   2                   3
				// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				// +-+-+-+-+-------+-+-------------+-------------------------------+
				// |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
				// |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
				// |N|V|V|V|       |S|             |   (if payload len==126/127)   |
				// | |1|2|3|       |K|             |                               |
				// +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
				// |     Extended payload length continued, if payload len == 127  |
				// + - - - - - - - - - - - - - - - +-------------------------------+
				// |                               |Masking-key, if MASK set to 1  |
				// +-------------------------------+-------------------------------+
				// | Masking-key (continued)       |          Payload Data         |
				// +-------------------------------- - - - - - - - - - - - - - - - +
				// :                     Payload Data continued ...                :
				// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
				// |                     Payload Data continued ...                |
				// +---------------------------------------------------------------+
				//
				//0x81 = 10000001 which says according to the table above:
				//       1        it's the final message 
				//        000     RSV1-3 are extensions which must be negotiated first
				//           0001 opcode %x1 denotes a text frame
				lb.Add(0x81);
				//0x04 = 00001000
				//       0        No mask
				//        0001000 Rest of the 7 bytes left is the length of the payload.
				lb.Add(0x04);
				// add the payload
				lb.AddRange(Encoding.UTF8.GetBytes("Test"));
				//write it!
				stream.Write (lb.ToArray(), 0, 6);
			}
		}
	}
}

