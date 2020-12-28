using System;
using System.Collections.Generic;
using System.Text;

using System.Runtime.InteropServices;

namespace test_dll
{
    class WozekClient
    {

        public delegate void EchoCallbackDelegate  	(IntPtr data, UInt32 size);
        public delegate void ErrorCallbackDelegate 	();
        public delegate void LookupCallbackDelegate	(Int32 id);

        // Available functions: 

        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr initialize();                // Initialize the application and return the handle (64-bit). there cannot be more than 1 application running at a time

        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void terminate(IntPtr handle);      // Terminate the applictation attached to the handle. Frees up memory

        // Connects to the specified server. Accepts null terminated strings as address (can be a domain name) and port
        // Returns a bool, determining wheater the connection was sucessfull

        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern bool connectToServer(IntPtr handle, string address, string port);

        // Disconnects from TCP server

        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void disconnectFromServer(IntPtr handle);


        // Asio is a (mostly) non blocking interface, so it has to be designated a thread or specifiad time, when it can execute


        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void pollOne(IntPtr handle); // Processes and runs execly one queued function (if none event handlers are ready for execution, returns immidiatly)

        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void pollAll(IntPtr handle); // Processes and runs all queued handlers ready for execution(if none are ready, returns immidietly)

        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void run(IntPtr handle);     // Processes handlers indefinetly, until stopped. Should be best tun on a seperate thread, or even multiple threads

        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void stop(IntPtr handle);    // Stops the polling and execution of all handlers. run() adn runOne() return as only they stop processing current handler, or immidiatly, if no work is being done

        // Bellow functions require a callback to be passed to them. Below functions and callbacks will be executed only in a thread with either pollOne() or run()
        // Callbacks need to be set only once

        // Sends an TCP/UDP Echo request. Callback will pass the pointer to the received response string and it's length via the parameter. If request failed, nullpointer will be passed. Ueed for testing.
        
        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void setTcpEchoCallback(IntPtr handle, IntPtr callback);
        
        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void sendTcpEcho(IntPtr handle, string message, UInt32 messageLength);
        

        
        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void setUdpEchoCallback(IntPtr handle, IntPtr callback);

        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void sendUdpEcho(IntPtr handle, string message, UInt32 messageLength);


        // Sends UDP Update State Request with given rotation for controller with given id
        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void setUdpUpdateStateErrorCallback(IntPtr handle, IntPtr callback);

        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void sendUdpUpdateState(IntPtr handle, UInt32 id, byte r1, byte r2, byte r3);
		
		
		// Sends TCP name lookup for given id
		// Callback is executed with argument: -1 - error, 0 - name dosen't exist, else - result id
        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
		public static extern void setTcpLookupIdForNameCallback( IntPtr handle, IntPtr callback);
		
        [DllImport("WozekHostClient.dll", CallingConvention = CallingConvention.StdCall)]
		public static extern void sendTcpLookupIdForName( IntPtr handle, IntPtr name, UInt32 nameLength);
    }
}
