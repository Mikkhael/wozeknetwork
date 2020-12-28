using System;
using System.IO;

using System.Runtime.InteropServices;
using System.Threading;

namespace test_dll
{
    class Program
    {
        static IntPtr handle;
        static void RunMethod()
        {
            WozekClient.run(handle);
        }
        static void EchoCallback(IntPtr data, UInt32 length)
        {
            if(length == 0 || data == IntPtr.Zero)
            {
                Console.WriteLine("Error occured during echo request");
                return;
            }
            string result = Marshal.PtrToStringAnsi(data, (int)length);
            Console.WriteLine("Length: " + length);
            Console.WriteLine("Ptr: " + data);
            Console.WriteLine("Received response: "+ result);
        }
        static void ErrorCallback()
        {
            Console.WriteLine("Error occured during sending the Update State Request");
        }

        static void Main(string[] args)
        {
            handle = WozekClient.initialize();

            WozekClient.EchoCallbackDelegate echoDelegate = EchoCallback;
            WozekClient.ErrorCallbackDelegate errorDelegate = ErrorCallback;

            WozekClient.setTcpEchoCallback(handle, Marshal.GetFunctionPointerForDelegate(echoDelegate));
            WozekClient.setUdpEchoCallback(handle, Marshal.GetFunctionPointerForDelegate(echoDelegate));
            WozekClient.setUdpUpdateStateErrorCallback(handle, Marshal.GetFunctionPointerForDelegate(errorDelegate));

            bool res = WozekClient.connectToServer(handle, "192.168.0.15", "8081");
            if(!res)
            {
                Console.WriteLine("Not connected");
                WozekClient.terminate(handle);
                return;
            }

            Console.WriteLine("Connected");

            string s1 = "tylko jedno w głowie mam";
            uint s1_len = (uint)s1.Length;
            string s2 = "koksu piec gram";
            uint s2_len = (uint)s2.Length;

            WozekClient.sendTcpEcho(handle, s1, s1_len);
            WozekClient.sendUdpEcho(handle, s2, s2_len);
            WozekClient.sendUdpUpdateState(handle, 1, 0, 120, 255);

            int op = 0;
            Console.Write("Chose option (1 - manual, 2 - thread): ");
            op = Console.Read();
            if (op == '1')
            {
                int end = 1;
                while (end != 'x')
                {
                    end = Console.Read();
                    WozekClient.pollOne(handle);
                }
            }
            else
            {

                Thread t = new Thread(new ThreadStart(RunMethod));
                Console.WriteLine("Starting thread");
                t.Start();

                Console.WriteLine("Awaiting read");
                Console.ReadKey();

                Console.WriteLine("Stoping execution");
                WozekClient.stop(handle);

                Console.WriteLine("Joining thread");
                t.Join();
            }

            WozekClient.terminate(handle);
        }
    }
}
