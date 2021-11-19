#pragma once 

#ifndef ORDERGEN_H
#define ORDERGEN_H

#include <mutex>
#include <random>
#include <thread>
#include <array>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <boost/log/trivial.hpp>
#include <fstream>
#include <string>
#include <sstream>


using namespace std::chrono_literals;
using namespace std::literals;
using namespace std::chrono;


class Order_Generator;

class Order {

public:

	Order() = default;

	Order(float px, int qty, int side, int guid, int type, bool GFD, bool IOC, std::string ISIN, std::string securityType)
		: m_price(px), m_quantity(qty), m_side(side), m_ID(guid), m_type(type), m_GFD(GFD), m_IOC(IOC),m_ISIN(ISIN), m_securityType(securityType) {

		//std::cout << "checking type 0 \n" << m_type;
		if (m_type == MARKET) {
			//std::cout << "Setting px 0 \n"; 
			m_price = 0;
		}
	};

	int getSide() const {
		return m_side;
	}

	std::string getISIN() const {
		return m_ISIN;
	}

	float getPrice() const {
		return m_price;
	}

	int getQty() const {
		return m_quantity;
	}

	int getType() const {
		return m_type;
	}
	int getID() const {
		return m_ID;
	}

	bool get_GFD_Status() const {
		return m_GFD;
	}

	bool getIOCStatus() const {
		return m_IOC;
	}

	std::chrono::time_point<std::chrono::steady_clock> getArrivalTime() {
		return m_arrivalTime;
	}

	void setQty(int newQty) {
		m_quantity = newQty;
	}
	void setArrivalTime() {
		m_arrivalTime = steady_clock::now();
	}

	void reset() {
		m_price = 0 ;
		m_quantity = 0 ;
		m_side = 0 ;
		m_ISIN = "" ;
		m_GFD = false;
		m_IOC = false;
		m_securityType = "";

	}
	~Order() = default;

	enum orderType {MARKET,LIMIT};

private:

	float	m_price{ 0 };
	int     m_quantity{ 0 };
	int		m_side{ 0 };
	int		m_type{ MARKET };
	int		m_ID;
	bool	m_GFD;
	bool	m_IOC;
	std::string m_ISIN;
	std::string m_securityType;
	std::chrono::time_point<std::chrono::steady_clock>  m_arrivalTime;		// Arrival at matching engine


};
namespace Orders {

	std::mutex orderBufferMutx1, orderBufferMutx2;
	std::vector<Order> orderBuffer;
	std::condition_variable cv1;
	std::condition_variable cv2;
	std::atomic<bool> bufferProcessed = true;
	std::atomic<bool> ordersReady = false;

}

extern class Order;

class Order_Generator {

public:
	Order_Generator() {

		guid = 0;
		BOOST_LOG_TRIVIAL(info) << "Order Generator: constructing order generator and allocating thread";
		t1 = std::thread(&Distribute_Orders);
		t1.join();

	}

	static void Distribute_Orders() {

		while (true) {

			Read_Input_File();
			std::this_thread::sleep_for(100s);
			//Generate_Order();
			//TEST_Generate_Buy_Market_Then_Sell_Limit();
			//TEST_Generate_2_Buy_Limit_Orders();
			//TEST_Generate_Sell_Market_Sell_Limit_Buy_Market_Orders();
			//TEST_Generate_3_Buy_Market_1_Sell_Limit_Orders();
			//TEST_Generate_4_Buy_Limit_1_Sell_Limit_Orders();
			//TEST_Generate_1_Buy_Limit_1_Sell_Market_Orders();
			//TEST_Generate_3_Sell_Limit_1_Buy_Market_Orders(
			//TEST_Generate_4_Sell_Limit_1_Buy_Limit_Orders();
			//TEST_Generate_Limit_Orders();
			//std::this_thread::sleep_for(1000s);


		}
	}

	static void Generate_Order() {

		try {

			std::random_device rd;						// randomize price/qty/side.. in order generation logic
			std::mt19937 gen(rd());
			std::uniform_int_distribution<int>		udist(100, 500);
			std::uniform_real_distribution<>		ureal(100, 300);
			std::uniform_int_distribution<>			udist2(0, ISINs.size() - 1);
			std::uniform_int_distribution<>			udist3(0, 1);
			std::uniform_int_distribution<>			udist4(0, 2);

			auto s = ISINs.size();
			float px = ureal(gen);
			int qty = udist(gen);
			int side = udist3(gen);
			int type = udist4(gen);
			if (type == 2) type = 1;
			int ID = ++guid;
			bool GFD = udist3(gen);
			bool IOC = false;
			//if (!GFD) {
			//	 IOC = udist3(gen);
			//}
			std::string ISIN = ISINs[udist2(gen)];

			Order ord(px, qty, side, guid, type, GFD, IOC, ISIN, "Equity");

			BOOST_LOG_TRIVIAL(info) << "Order Generator: Price " << ord.getPrice() << " Qty " << qty << " Side " << (side == 0 ? "BUY" : "SELL") 
									<< (type == 0 ? " MARKET" : " LIMIT") << " ISIN " << ISIN << " Order ID " << guid << " Type "		
									<< (GFD?" GFD ":" non-GFD ")  << (IOC?" IOC ":" non-IOC ") << std::endl;


			if (!Orders::ordersReady) {

				Orders::orderBuffer.push_back(ord);

				Orders::ordersReady = true;
				
			}

		}
		catch (std::exception e) {
			std::cout << e.what() << std::endl;
		}



	}

	static void Read_Input_File() {

		std::vector<std::string> input_vec;

		std::ifstream ifs("Inputfile.csv", std::ios::in);

		for (std::string line; std::getline(ifs, line);) {
			std::cout << " new line" << std::endl;
			std::istringstream ss(line);
			if (line[0] == '#') continue;

			for (std::string token; std::getline(ss, token, ',');) {
				//BOOST_LOG_TRIVIAL(info) << "\n\n ";
				//if (token != "") BOOST_LOG_TRIVIAL(info) << token << " ";
				if (token != "") {
					input_vec.push_back(token);
					std::cout << " push back " << token << std::endl;
				}
			}
			if (line[0] == 'N') {
				std::cout << "input 5 = :" << input_vec[5].substr(input_vec[5].find_first_not_of(' ')) << std::endl;
				Order ord1(std::stoi(input_vec[3]), std::stoi(input_vec[4]), (input_vec[5].substr(input_vec[5].find_first_not_of(' ')) == "B" ? 0 : 1), std::stoi(input_vec[6]), (line[3] == 0 ? 0 : 1), 0, 0, input_vec[2], "Equity");
				if (!Orders::ordersReady) {

					Orders::orderBuffer.push_back(ord1);
					Orders::ordersReady = true;
					//break;
				}
			}
			if (line[0] == 'F') {
				std::cout << "seen an F" << std::endl;
				Order ord2(0, 0, 0, 0, 99, 0, 0, "Flush", "Equity");					// Use order message with order type = 99 to indicate a flush message
				if (!Orders::ordersReady) {

					Orders::orderBuffer.push_back(ord2);
					Orders::ordersReady = true;
					//break;
				}
			}



			input_vec.clear();

		}

		BOOST_LOG_TRIVIAL(info) <<  std::endl;
	}

	static void TEST_Generate_1_Buy_Limit_1_Sell_Market_Orders() {

		Order ord1(160, 323, 0, 1, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord2(0, 470, 1, 2, 0, 0, 0, "GB0007980591"s, "Equity");

		BOOST_LOG_TRIVIAL(info) << "Order Generator: Price " << ord1.getPrice() << " Qty " << ord1.getQty() << " Side " << (ord1.getSide() == 0 ? "BUY" : "SELL")
			<< " Order ID " << ord1.getID() << " ISIN " << ord1.getISIN() << "Type " << (ord1.getType() == 0 ? "MARKET" : "LIMIT")
			<< (ord1.get_GFD_Status() ? " GFD " : " non-GFD ") << (ord1.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		if (!Orders::ordersReady) {

			Orders::orderBuffer.push_back(ord2);
			Orders::orderBuffer.push_back(ord1);
			Orders::ordersReady = true;

		}
	}
		
		
	static void generateBuyMarketThenSellLimit() {


		Order ord1(0,     196, 1, 1, 0, 0, 0, "GB0007980591"s, "Equity");
		Order ord2(225.1, 116, 0, 2, 1, 0, 0, "GB0007980591"s, "Equity");

		BOOST_LOG_TRIVIAL(info) << "Order Generator: Price " << ord1.getPrice() << " Qty " << ord1.getQty() << " Side " << (ord1.getSide() == 0 ? "BUY" : "SELL") 
			<< " Order ID " << ord1.getID() << " ISIN " << ord1.getISIN() << "Type " << (ord1.getType() == 0 ? "MARKET" : "LIMIT") 
			<< (ord1.get_GFD_Status() ? " GFD " : " non-GFD ") << (ord1.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		if (!Orders::ordersReady) {

			Orders::orderBuffer.push_back(ord2);
			Orders::orderBuffer.push_back(ord1);
			Orders::ordersReady = true;

		}
	}

	static void TEST_Generate_2_Buy_Limit_Orders() {

		Order ord1(141, 490, 0, 1, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord2(285.1, 337, 0, 2, 1, 0, 0, "GB0007980591"s, "Equity");

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord1.getPrice() << " Qty " << ord1.getQty() 
			<< " Side " << (ord1.getSide() == 0 ? "BUY" : "SELL")
			<< " Order ID " << ord1.getID() << " ISIN " << ord1.getISIN() << "Type " << (ord1.getType() == 0 ? "MARKET" : "LIMIT")
			<< (ord1.get_GFD_Status() ? " GFD " : " non-GFD ") << (ord1.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;  

		if (!Orders::ordersReady) {

			Orders::orderBuffer.push_back(ord2);
			Orders::orderBuffer.push_back(ord1);
			Orders::ordersReady = true;

		}
	}


	static void TEST_Generate_Limit_Orders() {

		Order ord1(280, 425,   1, 1, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord2(187, 430,   0, 2, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord3(224, 197,   1, 3, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord4(257, 199,   0, 4, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord5(113, 112,   0, 5, 1, 0, 0, "GB0007980591"s, "Equity");

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord1.getPrice() << " Qty " << ord1.getQty() 
			<< " Side " << (ord1.getSide() == 0 ? "BUY" : "SELL")
			<< " Order ID " << ord1.getID() << " ISIN " << ord1.getISIN() << "Type " << (ord1.getType() == 0 ? "MARKET" : "LIMIT")
			<< (ord1.get_GFD_Status() ? " GFD " : " non-GFD ") << (ord1.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		if (!Orders::ordersReady) {

			Orders::orderBuffer.push_back(ord5);
			Orders::orderBuffer.push_back(ord4);
			Orders::orderBuffer.push_back(ord3);
			Orders::orderBuffer.push_back(ord2);
			Orders::orderBuffer.push_back(ord1);
			Orders::ordersReady = true;

		}
	}


	static void TEST_Generate_Sell_Market_Sell_Limit_Buy_Market_Orders() {

		Order ord1(0, 233, 0, 1, 0, 0, 0, "GB0007980591"s, "Equity");
		Order ord2(263, 111, 0, 2, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord3(0, 299, 1, 3, 0, 0, 0, "GB0007980591"s, "Equity");


		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord1.getPrice() << " Qty " << ord1.getQty() << " Side " 
			<< (ord1.getSide() == 0 ? "BUY" : "SELL")  << " Order ID " << ord1.getID() << " ISIN " << ord1.getISIN() 
			<< "Type " << (ord1.getType() == 0 ? "MARKET" : "LIMIT") << (ord1.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord1.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord2.getPrice() << " Qty " << ord2.getQty() << " Side " 
			<< (ord2.getSide() == 0 ? "BUY" : "SELL") << " Order ID " << ord2.getID() << " ISIN " << ord2.getISIN() 
			<< "Type " << (ord2.getType() == 0 ? "MARKET" : "LIMIT")  << (ord2.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord2.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord3.getPrice() << " Qty " << ord3.getQty() << " Side " 
			<< (ord3.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord3.getISIN() << "Type " 
			<< (ord3.getType() == 0 ? "MARKET" : "LIMIT") << (ord3.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord3.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		if (!Orders::ordersReady) {

			Orders::orderBuffer.push_back(ord3);
			Orders::orderBuffer.push_back(ord2);
			Orders::orderBuffer.push_back(ord1);
			Orders::ordersReady = true;

		}
	}

	static void TEST_Generate_4_Buy_Limit_1_Sell_Limit_Orders() {


		Order ord1(156, 70, 0, 1, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord2(225, 182, 0, 2, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord3(234, 300, 0, 3, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord4(100, 50, 0, 4, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord5(153, 555, 1, 5, 1, 0, 0, "GB0007980591"s, "Equity");


		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord1.getPrice() << " Qty " << ord1.getQty() 
			<< " Side " << (ord1.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord1.getISIN()
			<< "Type " << (ord1.getType() == 0 ? "MARKET" : "LIMIT") << (ord1.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord1.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord2.getPrice() << " Qty " << ord2.getQty() 
			<< " Side " << (ord2.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord2.getISIN()
			<< "Type " << (ord2.getType() == 0 ? "MARKET" : "LIMIT") << (ord2.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord2.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord3.getPrice() << " Qty " << ord3.getQty() 
			<< " Side " << (ord3.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord3.getISIN()
			<< "Type " << (ord3.getType() == 0 ? "MARKET" : "LIMIT") << (ord3.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord3.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord4.getPrice() << " Qty " << ord4.getQty() 
			<< " Side " << (ord4.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord4.getISIN()
			<< "Type " << (ord4.getType() == 0 ? "MARKET" : "LIMIT") << (ord4.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord4.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord5.getPrice() << " Qty " << ord5.getQty() 
			<< " Side " << (ord5.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord5.getISIN()
			<< "Type " << (ord5.getType() == 0 ? "MARKET" : "LIMIT") << (ord5.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord5.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;


		if (!Orders::ordersReady) {

			Orders::orderBuffer.push_back(ord5);
			Orders::orderBuffer.push_back(ord4);
			Orders::orderBuffer.push_back(ord3);
			Orders::orderBuffer.push_back(ord2);
			Orders::orderBuffer.push_back(ord1);
			Orders::ordersReady = true;

		}
	}

	static void TEST_Generate_4_Sell_Limit_1_Buy_Limit_Orders() {


		Order ord1(256, 70, 1, 1, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord2(225, 182, 1, 2, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord3(234, 300, 1, 3, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord4(300, 50, 1, 4, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord5(305, 555, 0, 5, 1, 0, 0, "GB0007980591"s, "Equity");


		BOOST_LOG_TRIVIAL(info) << "Order Generator: Price " << ord1.getPrice() << " Qty " << ord1.getQty() 
			<< " Side " << (ord1.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord1.getISIN()
			<< "Type " << (ord1.getType() == 0 ? "MARKET" : "LIMIT") << (ord1.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord1.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) << "Order Generator: Price " << ord2.getPrice() << " Qty " << ord2.getQty() 
			<< " Side " << (ord2.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord2.getISIN()
			<< "Type " << (ord2.getType() == 0 ? "MARKET" : "LIMIT") << (ord2.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord2.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) << "Order Generator: Price " << ord3.getPrice() << " Qty " << ord3.getQty() 
			<< " Side " << (ord3.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord3.getISIN()
			<< "Type " << (ord3.getType() == 0 ? "MARKET" : "LIMIT") << (ord3.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord3.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) << "Order Generator: Price " << ord4.getPrice() << " Qty " << ord4.getQty() 
			<< " Side " << (ord4.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord4.getISIN()
			<< "Type " << (ord4.getType() == 0 ? "MARKET" : "LIMIT") << (ord4.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord4.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) << "Order Generator: Price " << ord5.getPrice() << " Qty " << ord5.getQty() 
			<< " Side " << (ord5.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord5.getISIN()
			<< "Type " << (ord5.getType() == 0 ? "MARKET" : "LIMIT") << (ord5.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord5.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;


		if (!Orders::ordersReady) {

			Orders::orderBuffer.push_back(ord5);
			Orders::orderBuffer.push_back(ord4);
			Orders::orderBuffer.push_back(ord3);
			Orders::orderBuffer.push_back(ord2);
			Orders::orderBuffer.push_back(ord1);
			Orders::ordersReady = true;

		}
	}

	static void TEST_Generate_3_Sell_Limit_1_Buy_Market_Orders() {

		Order ord1(292, 27,  1, 1, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord2(131, 46,  1, 2, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord3(251, 259, 1, 3, 1, 0, 0, "GB0007980591"s, "Equity");
		Order ord4(0,   326, 0, 4, 0, 0, 0, "GB0007980591"s, "Equity");
		//Order ord5(153, 355, 1, 5, 1, 0, 0, "GB0007980591"s, "Equity");

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord1.getPrice() << " Qty " << ord1.getQty() 
			<< " Side " << (ord1.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord1.getISIN()
			<< "Type " << (ord1.getType() == 0 ? "MARKET" : "LIMIT") << (ord1.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord1.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord2.getPrice() << " Qty " << ord2.getQty() 
			<< " Side " << (ord2.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord2.getISIN()
			<< "Type " << (ord2.getType() == 0 ? "MARKET" : "LIMIT") << (ord2.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord2.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord3.getPrice() << " Qty " << ord3.getQty() 
			<< " Side " << (ord3.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord3.getISIN()
			<< "Type " << (ord3.getType() == 0 ? "MARKET" : "LIMIT") << (ord3.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord3.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) << "Order Generator: Price " << ord4.getPrice() << " Qty " << ord4.getQty() 
			<< " Side " << (ord4.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord4.getISIN()
			<< "Type " << (ord4.getType() == 0 ? "MARKET" : "LIMIT") << (ord4.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord4.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		//BOOST_LOG_TRIVIAL(info) << "Order Generator: Price " << ord5.getPrice() << " Qty " << ord5.getQty() << " Side " << (ord5.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord5.getISIN()
		//	<< "Type " << (ord5.getType() == 0 ? "MARKET" : "LIMIT") << (ord5.get_GFD_Status() ? " GFD " : " non-GFD ") << (ord5.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;


		if (!Orders::ordersReady) {

			//orderBuffer.push_back(ord5);
			Orders::orderBuffer.push_back(ord4);
			Orders::orderBuffer.push_back(ord3);
			Orders::orderBuffer.push_back(ord2);
			Orders::orderBuffer.push_back(ord1);
			Orders::ordersReady = true;

		}
	}

	static void TEST_Generate_3_Buy_Market_1_Sell_Limit_Orders() {

		Order ord1(0, 68, 1, 1, 0, 0, 0, "GB0007980591"s, "Equity");
		Order ord2(0, 123, 1, 2, 0, 0, 0, "GB0007980591"s, "Equity");
		Order ord3(0, 421, 1, 3, 0, 0, 0, "GB0007980591"s, "Equity");
		Order ord4(175, 199, 0, 4, 1, 0, 0, "GB0007980591"s, "Equity");


		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord1.getPrice() << " Qty " << ord1.getQty() 
			<< " Side " << (ord1.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord1.getISIN()
			<< "Type " << (ord1.getType() == 0 ? "MARKET" : "LIMIT") << (ord1.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord1.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord2.getPrice() << " Qty " << ord2.getQty() 
			<< " Side " << (ord2.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord2.getISIN()
			<< "Type " << (ord2.getType() == 0 ? "MARKET" : "LIMIT") << (ord2.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord2.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		BOOST_LOG_TRIVIAL(info) 
			<< "Order Generator: Price " << ord3.getPrice() << " Qty " << ord3.getQty() 
			<< " Side " << (ord3.getSide() == 0 ? "BUY" : "SELL") << " ISIN " << ord3.getISIN()
			<< "Type " << (ord3.getType() == 0 ? "MARKET" : "LIMIT") << (ord3.get_GFD_Status() ? " GFD " : " non-GFD ") 
			<< (ord3.getIOCStatus() ? " IOC " : " non-IOC ") << std::endl;

		if (!Orders::ordersReady) {

			Orders::orderBuffer.push_back(ord4);
			Orders::orderBuffer.push_back(ord3);
			Orders::orderBuffer.push_back(ord2);
			Orders::orderBuffer.push_back(ord1);
			Orders::ordersReady = true;

		}
	}

private:

	std::thread t1;
	inline static std::array<std::string, 5> ISINs{ "GB0007980591"s }; //Expand to "GB00B03MLX29"s, "DE0005190003"s, "ES0178430E18"s, "FR0000131104"s};
	enum orderType { MARKET, LIMIT };
	inline static int guid;

};

#endif