#include <iostream>
#include <thread>
#include <memory>
#include <atomic>
#include <chrono>

#include <bitset>

#include "seriallib.hpp"
#include "Device.hpp"

#include "utils.hpp"

#include "ThreadedQueues.hpp"

serialib serial;

std::atomic<int> expect = 0;

void readOnce()
{
	char buffer;
	int err;
	
	if(expect.load(std::memory_order::memory_order_relaxed) == 2)
	{
		err = serial.readChar(&buffer, 15);
		std::cout << "RE: " << static_cast<int>(buffer) << "(" << err << ")\n";
		expect.store(0, std::memory_order::memory_order_relaxed);
		return;
	}
	if(expect.load(std::memory_order::memory_order_relaxed) != 0)
	{
		return;
	}
	while(serial.available())
	{
		err = serial.readChar(&buffer);
		std::cout << "R: " << static_cast<int>(buffer) << "(" << err << ")\n";
	}
}

void readF()
{
	while(true)
	{
		readOnce();
		//std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}


void controlF()
{
	int err;
	int value;
	while(true)
	{
		std::cin >> value;
		
		if(value == 400)
		{
			err = serial.openDevice("COM4", 9600);
			std::cout << "Opening device: " << err << '\n';
			continue;
		}
		if(value == 300)
		{
			serial.closeDevice();
			std::cout << "Closing device\n";
			continue;
		}
		
		if(value == 303)
		{
			const char bytes[] {1, 2};
			const char b = 3;
			serial.writeChar(b);
			serial.writeBytes(bytes, 2);
			continue;
		}
		
		if(value == 312)
		{
			const char b = 12;
			const float ds[] {1, 0, 0.5};
			serial.writeChar(b);
			serial.writeBytes(ds, 12);
			continue;
			
		}
		
		if(value == 5 || value == 6 || value == 7 || value == 8)
		{
			expect.store(1, std::memory_order::memory_order_relaxed);
		}
		
		err = serial.writeChar(char(value));
		std::cout << "Writing char ("<< value << "): " << err << '\n';
		
		if(value == 5 || value == 6 || value == 7 || value == 8)
		{
			expect.store(2, std::memory_order::memory_order_relaxed);
		}
		
		
		continue;
	}
}

#define time(x) std::chrono::duration_cast<std::chrono::microseconds>(x).count()
#define NOW std::chrono::high_resolution_clock::now()

void ping()
{
	serial.openDevice("COM4", 9600);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	
	
	float f[] {0, 1, 0, 0.4143};
	char* w = (char*)f;
	w += 3;
	*w = 12;
	int wsize = 13;
	char c[512];
	while(true)
	{
		serial.writeBytes(w, wsize);
		auto t0 = std::chrono::high_resolution_clock::now();
		while(!serial.available());
		auto t1 = std::chrono::high_resolution_clock::now();
		auto res = serial.readBytes(&c, 512, 1);
		//auto res = serial.readChar(c, 1);
		auto t2 = std::chrono::high_resolution_clock::now();
		
		std::cout << "Value in buffer: " << time(t1 - t0) << "\t";
		std::cout << "Value read: " << time(t2 - t1) << "\t";
		std::cout << "Total time: " << time(t2 - t0) << '\t' << res << '\t' << (int)c[0] << "\n";
		
		//std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

void timeEncodes()
{
	
	
	constexpr unsigned int is = 10000000;
	
	
	
	byte data[5] {25, 34, 28, 255, 12};
	
	// 25 34 28 255 12
	// b1 49 8 39 7f 6 0
	
	// 255 255 255 255 255
	std::cout << "Input bytes: \n";
	for(int i=0; i<sizeof(data); i++)
	{
		int temp;
		std::cin >> temp;
		data[i] = temp;
	}
	
	constexpr int resSize = getEncodedLength(sizeof(data));
	byte* res = new byte[is * resSize];
	
	
	auto t0 = NOW;
	for(unsigned int i=0; i<is; i++)
	{
		//encode<3, sizeof(data)>(data, res + i * resSize);
		encode(3, sizeof(data), data, res + i * resSize);
	}
	auto t1 = NOW;
	for(unsigned int i=0; i<is; i++)
	{
		encode<3, sizeof(data)>(data, res + i * resSize);
		//encode(3, sizeof(data), data, res + i * resSize);
	}
	auto t2 = NOW;
	
	auto d1 = time(t1 - t0) / float(1000);
	auto d2 = time(t2 - t1) / float(1000);
	std::cout << "Simple function: \t" << d1 << " ms\nTemplated function:\t" << d2 << " ms\n\n";
	
		lookupBits(res, resSize);
		std::cout << '\t';
		lookupBytes(res, resSize);
}

void testEncodes2()
{
	
	float t[] {1, 1, 1, 1};
	byte data[sizeof(t)] {};
	
	byte edata[getEncodedLength(sizeof(data))]{};
	byte ddata[sizeof(data)] {};
	
	t[0] = 0;
	t[1] = 0;
	t[2] = 0;
	t[3] = 0;
	memcpy(data, (char*)t, sizeof(t) );
	
	std::cout << "--- " << t[0] << "  " << t[1] << "  " << t[2] << "  " << t[3] << '\n';
	lookupBytes(data, sizeof(data)); std::cout << '\n';
	encode<4, sizeof(data)>(data, edata);
	//encode(4, sizeof(data), data, edata);
	decode(sizeof(data), edata, ddata);
	lookupBytes(ddata, sizeof(ddata)); std::cout << '\n';
	lookupBytes(edata, sizeof(edata)); std::cout << '\n';
	
	
	for(int i=0; i<sizeof(data); i++)
	{
		data[i] = 255;
	}
	memcpy( (char*)t, data, sizeof(t) );	
	
	std::cout << "--- " << t[0] << "  " << t[1] << "  " << t[2] << "  " << t[3] << '\n';
	lookupBytes(data, sizeof(data)); std::cout << '\n';
	encode<4, sizeof(data)>(data, edata);
	//encode(4, sizeof(data), data, edata);
	decode(sizeof(data), edata, ddata);
	lookupBytes(ddata, sizeof(ddata)); std::cout << '\n';
	lookupBytes(edata, sizeof(edata)); std::cout << '\n';
	
	t[0] = 0;
	t[1] = 0;
	t[2] = 0;
	t[3] = 0;
	memcpy(data, (char*)t, sizeof(t) );
	
	std::cout << "--- " << t[0] << "  " << t[1] << "  " << t[2] << "  " << t[3] << '\n';
	lookupBytes(data, sizeof(data)); std::cout << '\n';
	encode<4, sizeof(data)>(data, edata);
	//encode(4, sizeof(data), data, edata);
	decode(sizeof(data), edata, ddata);
	lookupBytes(ddata, sizeof(ddata)); std::cout << '\n';
	lookupBytes(edata, sizeof(edata)); std::cout << '\n';
	
	
}

void tak()
{
	
	/*
	std::thread writerThread(controlF);
	std::thread readerThread(readF);
	
	writerThread.join();
	readerThread.join();*/
}

void testDecode()
{
	float t[3] {1, 1, 1};
	byte data[sizeof(t)] {};
	
	
	//byte data[30] {255, 255, 255};
	
	memcpy(data, (char*)t, sizeof(t) );
	
	std::cout << "Start: " << std::hex << (long long)(data) << "\nEnd:   " <<  std::hex << (long long)(data+sizeof(data)) << '\n';
	
	for(int i=0; i< sizeof(data); i++) data[i] = rand();
	
	byte edata[getEncodedLength(sizeof(data))]{};
	byte ddata[sizeof(data)] {};
	
	std::cout << "Start: " <<  std::hex << (long long)(edata) << "\nEnd:   " <<  std::hex << (long long)(edata+sizeof(edata)) << '\n//';
	
	//encode<4, sizeof(data)>( data, edata);
	encode(4, sizeof(data), data, edata);
	decode(sizeof(data), edata, ddata);
	
	lookupBytes(data, sizeof(data)); std::cout << '\n';
	lookupBytes(ddata, sizeof(ddata)); std::cout << '\n';
	lookupBytes(edata, sizeof(edata)); std::cout << '\n';
	
	//lookupBits(ddata, sizeof(ddata)); std::cout << '\n';
	//lookupBits(edata, sizeof(edata)); std::cout << '\n';
	
	for(int i=0; i<sizeof(data); i++)
	{
		if(data[i] != ddata[i])
		{
			std::cout << "ERR: " << i << '\n';
		}
	}
	std::cout << "End\n";
}


void testQueue()
{
	{
		MutexCopyQueue<std::string> q;
		
		std::thread t1([&]{
			std::clog << "S: q1\n";
			q.push("q1");
			std::clog << "S: q2\n";
			q.push("q2");
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			std::clog << "S: q3\n";
			q.push("q3");
		});
		
		std::thread t2([&]{
			std::clog << "S: q4\n";
			q.push("q4");
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			std::clog << "S: q5\n";
			q.push("q5");
			std::clog << "S: q6\n";
			q.push("q6");
			std::clog << "S: q7\n";
			q.push("q7");
		});
		
		bool tak = true;
		
		std::thread t3([&]{
			while(tak)
			{
				auto r = q.top();
				if(r)
				{
					q.pop();
					std::clog << "R: " << r.value() << '\n';
				}
			}
		});
		
		
		std::thread t4([&]{
			char x;
			std::cin >> x;
			tak = false;
		});
		
		t4.join();
		t3.join();
		t2.join();
		t1.join();
	}
	
	char x;
	std::cin >> x;
	
	
}

int main()
{
	testQueue();
	
}

