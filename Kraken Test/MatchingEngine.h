#pragma once 

#ifndef MATCHING_H
#define MATCHING_H

#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <string>
#include <unordered_map>
#include <boost/log/trivial.hpp>
#include <variant>

#define LOG(x) std::cout << x <<std::endl;

extern class Order;

namespace Orders {

	extern std::mutex orderBufferMutx1, orderBufferMutx2;			// Not in use
	extern std::vector<Order> orderBuffer;
	extern std::condition_variable cv1, cv2;						// Not in use
	extern std::atomic<bool> bufferProcessed;						// Using atomics for this project
	extern std::atomic<bool> ordersReady;							// Using atomics for this project
	std::atomic<bool> Order_Book_In_Use{ false };

}

enum side {BUY,SELL};

class orderBook1Symbol {

public:

	orderBook1Symbol() = default;

	orderBook1Symbol(std::string s) : 
		myISIN(s)  
	{

		Limit_Order_Book.first.resize(10);								// Spec the initial buy order book size - note it will grow organically
		Limit_Order_Book.second.resize(10);								// Spec the initial sell order book size - note it will grow organically
		Allocation_Vec_Buys.resize(50);									// Spec the initial buy allocation vector size
		Allocation_Vec_Sells.resize(50);								// Spec the initial buy allocation vector size
		Market_Orders_Allocation_Vec_Buys.resize(50);					// Spec the initial buy allocation vector for market orders size
		Market_Orders_Allocation_Vec_Sells.resize(50);					// Spec the initial sell allocation vector for market orders size

	}


	void print(const std::vector<Order>& vec) {
		for (auto& elem : vec) {
			std::cout << elem.getPrice() << "/" << elem.getQty() << "   ";
		}
		std::cout << std::endl;
	}

	void Add_Order_to_Book(Order ord, bool checked) {

		if (Orders::Order_Book_In_Use.load() == false) {
			Orders::Order_Book_In_Use = true;
		}

		auto side = ord.getSide();
		auto price = ord.getPrice();
		auto ordType = ord.getType();
		auto ordIOC = ord.getIOCStatus();

		int matched = 0;												// Design = Use a linear search from top of order book for insertion of new order

		if (!checked)  matched = Check_For_Match(ord);				// Checked signifies we have already checked for a match so just insert order into book
																	// matched signifies we found a full match and nothing further to add to book
		if (!matched && ordIOC == true) {
			ord.~Order();
			BOOST_LOG_TRIVIAL(info) << "Deleting MARKET IOC Order   " << std::endl;
			return;
		}

		if ((side == BUY) && (!matched)) {

			// Bids are listed starting from highest bid downwards	

			if (ordType == MARKET) {

				Market_Orders_Buys_Vec.push_back(ord);					//Market orders cannot be entered into the LIMIT book so held in a holding vector

				BOOST_LOG_TRIVIAL(info) << "Matching Engine:  inserting BUY MARKET order into holding BIDS vector " << price << std::endl;
				BOOST_LOG_TRIVIAL(info) << "Bids MARKET Order book view :   " << std::endl;

				print(Market_Orders_Buys_Vec);

				return;
			}

			BOOST_LOG_TRIVIAL(info) << "Matching Engine:  running Add_Order_to_Book for BUY LIMIT order  -- Current_Bids_Depth  " << Current_Bids_Depth << std::endl;

			if (Limit_Order_Book.first[0].getPrice() == 0) {

				Limit_Order_Book.first.insert(begin(Limit_Order_Book.first), ord);

				BOOST_LOG_TRIVIAL(info) << "Matching Engine:  inserting BUY LIMIT order into empty BIDS order book " << price << std::endl;
				BOOST_LOG_TRIVIAL(info) << "Bids LIMIT Order book view :   " << std::endl;

				print(Limit_Order_Book.first);

				BOOST_LOG_TRIVIAL(info) << "Matching Engine: Current_Bids_Depth = " << ord.getISIN() << " " << ++Current_Bids_Depth << std::endl << std::endl;
			}
			else { Insert_Limit_Order_Into_Book(BUY, price, ord); }

		}

		else {										// order is SELL - asks are listed so top of the book has highest index in vector

			if (!matched) {

				if (ordType == MARKET) {
					Market_Orders_Sells_Vec.push_back(ord);				//Market orders cannot be entered into the book so held in a holding vector

					BOOST_LOG_TRIVIAL(info) << "Matching Engine:  inserting SELL MARKET order into holding ASKS vector " << price << std::endl;
					BOOST_LOG_TRIVIAL(info) << "Asks MARKET Order book view :   " << std::endl;

					print(Market_Orders_Sells_Vec);

					return;
				}

				BOOST_LOG_TRIVIAL(info) << "Matching Engine: running Add_Order_to_Book   -- Current_Asks_Depth  " << Current_Asks_Depth << std::endl;

				if (Limit_Order_Book.second[0].getPrice() == 0) {								// No orders inserted into LIMIT book yet

					Limit_Order_Book.second.insert(begin(Limit_Order_Book.second), ord);

					BOOST_LOG_TRIVIAL(info) << "Matching Engine:  inserting SELL LIMIT order into empty ASKS order book " << price << std::endl;
					BOOST_LOG_TRIVIAL(info) << "Asks LIMIT Order book view :   " << std::endl;

					print(Limit_Order_Book.second);

					BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Current_Asks_Depth = " << ord.getISIN() << " " << ++Current_Asks_Depth << std::endl << std::endl;
				}

				else { Insert_Limit_Order_Into_Book(SELL, price, ord); }

			}

		}
		Orders::Order_Book_In_Use = false;

	}

	void Insert_Limit_Order_Into_Book(int side, float price, const Order& ord) {

		switch (side) {

		case BUY: {
			for (int i = std::max(0, Current_Bids_Depth - 1); i >= 0; i--) {


				auto priceAtLeveli = Limit_Order_Book.first[i].getPrice();
				//std::cout << "\npriceAtLeveli = " << priceAtLeveli << "  " << i << std::endl;

				if (Current_Bids_Depth == Max_Order_Book_Size) {

					BOOST_LOG_TRIVIAL(info) << "Matching Engine: Maximum bids Order book depth has been reached - no further orders are being accepted.";
					return;
				}
				if (priceAtLeveli < price) {

					BOOST_LOG_TRIVIAL(info) << "Matching Engine:  inserting LIMIT order priced at " << price << " at index i = " << i << std::endl;

					Limit_Order_Book.first.insert(begin(Limit_Order_Book.first) + i + 1, ord);
					BOOST_LOG_TRIVIAL(info) << "Bids LIMIT Order book view :   " << std::endl;

					print(Limit_Order_Book.first);

					BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Current_Bids_Depth = " << ord.getISIN() << " " << ++Current_Bids_Depth << std::endl;
					return;
				}
				if (priceAtLeveli > price) {

					if (i == 0) {										// we have reached bottom of the orderbook
						BOOST_LOG_TRIVIAL(info) << "Matching Engine:  inserting LIMIT order priced at " << price << " at index i = 0   " << std::endl;
						Limit_Order_Book.first.insert(begin(Limit_Order_Book.first), ord);
						BOOST_LOG_TRIVIAL(info) << "Bids LIMIT Order book view :   " << std::endl;
						print(Limit_Order_Book.first);
						BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Current_Bids_Depth = " << ord.getISIN() << " " << ++Current_Bids_Depth << std::endl;
						return;
					}

				}
				else if (priceAtLeveli == price) {

					BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Prices equal - inserting " << price << "   at index " << i << std::endl;
					Limit_Order_Book.first.insert(begin(Limit_Order_Book.first) + i, ord);
					print(Limit_Order_Book.first);
					BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Current_Bids_Depth = " << ord.getISIN() << " " << ++Current_Bids_Depth << std::endl;
					return;

				}
			}

		}
		case SELL: {
			for (int i = std::max(0, Current_Asks_Depth - 1); i >= 0; i--) {

				auto priceAtLeveli = Limit_Order_Book.second[i].getPrice();
				//std::cout << "\npriceAtLeveli = " << priceAtLeveli << "  " << i << std::endl;

				if (Current_Asks_Depth == Max_Order_Book_Size) {
					BOOST_LOG_TRIVIAL(info) << "Matching Engine: Maximum asks Order book depth has been reached - no further orders are being accepted.";
					return;
				}
				if (priceAtLeveli > price) {

					BOOST_LOG_TRIVIAL(info) << "Matching Engine: Inserting LIMIT order priced at " << price << " into book at index i = " << i << std::endl;
					Limit_Order_Book.second.insert(begin(Limit_Order_Book.second) + i + 1, ord);
					BOOST_LOG_TRIVIAL(info) << "Asks LIMIT Order book view :   " << std::endl;
					print(Limit_Order_Book.second);
					BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Current_Asks_Depth = " << ord.getISIN() << " " << ++Current_Asks_Depth << std::endl;
					return;
				}
				if (priceAtLeveli < price) {

					if (i == 0) {										// we have reached bottom of the orderbook
						BOOST_LOG_TRIVIAL(info) << "Matching Engine: inserting " << price << "   at index i = 0   " << std::endl;
						Limit_Order_Book.second.insert(begin(Limit_Order_Book.second), ord);
						BOOST_LOG_TRIVIAL(info) << "Asks LIMIT Order book view :   " << std::endl;
						print(Limit_Order_Book.second);
						BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Current_Asks_Depth = " << ord.getISIN() << " " << ++Current_Asks_Depth << std::endl;
						return;
					}

				}
				else if (priceAtLeveli == price) {

					BOOST_LOG_TRIVIAL(info) << "Matching Engine: Prices equal - inserting " << price << std::endl;
					Limit_Order_Book.second.insert(begin(Limit_Order_Book.second) + i, ord);
					print(Limit_Order_Book.second);
					BOOST_LOG_TRIVIAL(info) << "Matching Engine: Current_Asks_Depth = " << ord.getISIN() << " " << ++Current_Asks_Depth << std::endl;
					return;
				}
			}
		}
		}
	}


	int  Check_For_Match(const Order& ord) {


		BOOST_LOG_TRIVIAL(info) << "Matching Engine: Checking for match " << ord.getISIN() << " Current_Asks_Depth= " << Current_Asks_Depth << " Current_Bids_Depth= " << Current_Bids_Depth << std::endl;


		auto ordType = ord.getType();
		auto ordPrice = ord.getPrice();
		auto ordQty = ord.getQty();
		auto ordSide = ord.getSide();
		auto ordID = ord.getID();

		if ((ordSide == BUY && Current_Asks_Depth == 0 && Market_Orders_Sells_Vec.empty()) ||						// No present liquidity
			(ordSide == SELL && Current_Bids_Depth == 0 && Market_Orders_Buys_Vec.empty())) return 0;

		switch (ordSide) {

		case BUY: {
			// Sufficient liquidity at top of book

			BOOST_LOG_TRIVIAL(info) << "Matching Engine: Analysing SELL side liquidity " << std::endl;

			auto Top_Of_Asks_Book_Qty = Limit_Order_Book.second[std::max(0, Current_Asks_Depth - 1)].getQty();
			auto bestAsk = Limit_Order_Book.second[std::max(0, Current_Asks_Depth - 1)].getPrice();

			// Sufficient liquidity at top of bids book or markets book
			if ((ordType == LIMIT && ordPrice >= bestAsk && ordQty <= Top_Of_Asks_Book_Qty) || (ordType == MARKET && ordQty <= Top_Of_Asks_Book_Qty)) {

				Execute_Trade(ordID, bestAsk, ordQty);
				Limit_Order_Book.second[std::max(0, Current_Asks_Depth - 1)].setQty(Top_Of_Asks_Book_Qty - ordQty);

				BOOST_LOG_TRIVIAL(info) << "Matching Engine: Matching from top of asks book - Order book view =" << std::endl;

				print(Limit_Order_Book.second);
				return 1;

			}

			if (ordType == LIMIT && (Current_Asks_Depth == 0 || bestAsk > ordPrice)) {									// Nothing in the LIMIT book to match against so match against market orders

				auto res = Sweep_And_Match_Market_Orders(ord);
				return res;
			}

			if (ordType == MARKET && Top_Of_Asks_Book_Qty == 0) {

				return 0;
			}

			if ((ordType == LIMIT && ordPrice >= bestAsk && ordQty > Top_Of_Asks_Book_Qty) || (ordType == MARKET && ordQty > Top_Of_Asks_Book_Qty && Top_Of_Asks_Book_Qty != 0)) {

				auto tally = Top_Of_Asks_Book_Qty;										// Tally of liquidity consumed by incoming order
				//std::vector<std::pair<float, int>> allocationVec(50);					// Temp vector to hold mappings of prices to quantity consumed as we traverse down the order book
				Allocation_Vec_Buys[0].first = bestAsk;
				Allocation_Vec_Buys[0].second = Top_Of_Asks_Book_Qty;

				int residualVol{ 0 };													// Any liquidity left resting on the last touched order after sweeping through book
				int residual_Vol_Index{ 0 };
				int partialQty{ 0 };
				int leavesQty{ ordQty - tally };
				int fullExecs{ 1 };

				for (int i = 1; i < Max_Order_Book_Size; i++) {							// Sweep through order book from second top-of-book order until we have obtained the liquidity required

					if (Current_Asks_Depth - 1 - i < 0) {

						BOOST_LOG_TRIVIAL(info) << "Matching Engine: SELL side Order book exhausted " << std::endl;

						if (leavesQty) {												// If a partial order remaining after sweeping, load into the order book
							Order newOrd = ord;

							newOrd.setQty(leavesQty);
							if (!Market_Orders_Sells_Vec.empty() && ordType == LIMIT) {

								Sweep_And_Match_Market_Orders(newOrd);
							}
							else
								Add_Order_to_Book(newOrd, true);
						}
						break;
					}

					BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Allocating new order against multiple SELL orders  " << i << std::endl;

					Allocation_Vec_Buys[i].first = Limit_Order_Book.second[Current_Asks_Depth - 1 - i].getPrice();

					if ((Allocation_Vec_Buys[i].first <= ordPrice) || (ordType == MARKET)) {

						Allocation_Vec_Buys[i].second = Limit_Order_Book.second[Current_Asks_Depth - 1 - i].getQty();
						leavesQty = std::max(0, leavesQty - Allocation_Vec_Buys[i].second);

						BOOST_LOG_TRIVIAL(info) << "Matching Engine:  leavesQty  " << leavesQty << std::endl;
						auto newTally = tally + Allocation_Vec_Buys[i].second;
						//std::cout << "\nnewTally " << newTally << std::endl;

						if (newTally <= ordQty) fullExecs++;
						else {															// (newTally > ordQty)
							partialQty = ordQty - tally;								// Need to handle last partial allocate
							residual_Vol_Index = i;										// Track the index into the book
							residualVol = Allocation_Vec_Buys[i].second - partialQty;		// Calculate leftover volume to update that order in the book
							Allocation_Vec_Buys[i].second = partialQty;
							break;
						}
						tally = newTally;
					}
					else  Allocation_Vec_Buys[i].first = 0;								// Sale price not low enough for LIMIT so zero out that price
				}
				for (int j = 0; j < Allocation_Vec_Buys.size(); j++) {							// Send an exec report for each partial exec

					if (Allocation_Vec_Buys[j].second) {

						Execute_Trade(ordID, Allocation_Vec_Buys[j].first, Allocation_Vec_Buys[j].second);		// Could add a parent trade notification for the overall execution
						Allocation_Vec_Buys[j].second = 0;
					}
				}
				BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Removing " << fullExecs << " from SELL order book " << std::endl;

				Remove_From_Limit_Order_Book(SELL, fullExecs, residualVol, residual_Vol_Index);
				return 1;
			}

			else { std::cout << "Control flow not caught" << std::endl; return 0; }

		}

		case SELL: {


			BOOST_LOG_TRIVIAL(info) << "Matching Engine: Analysing BUY side liquidity " << std::endl;

			auto Top_Of_Bids_Book_Qty = Limit_Order_Book.first[std::max(0, Current_Bids_Depth - 1)].getQty();
			auto bestBid = Limit_Order_Book.first[std::max(0, Current_Bids_Depth - 1)].getPrice();


			// Sufficient liquidity at top of bids book
			if ((ordType == LIMIT && ordPrice <= bestBid && ordQty <= Top_Of_Bids_Book_Qty)) {

				Execute_Trade(ordID, bestBid, ordQty);
				Limit_Order_Book.first[std::max(0, Current_Bids_Depth - 1)].setQty(Top_Of_Bids_Book_Qty - ordQty);
				BOOST_LOG_TRIVIAL(info) << "Matching Engine: Matching from top of bids book - Order book view =" << std::endl;
				print(Limit_Order_Book.first);
				return 1;

			}

			if (ordType == MARKET && Top_Of_Bids_Book_Qty == 0) {

				return 0;
			}

			if (ordType == MARKET && ordQty <= Top_Of_Bids_Book_Qty) {

				Execute_Trade(ordID, bestBid, ordQty);
				Limit_Order_Book.first[std::max(0, Current_Bids_Depth - 1)].setQty(Top_Of_Bids_Book_Qty - ordQty);
				BOOST_LOG_TRIVIAL(info) << "Matching Engine: Matching from top of LIMIT book- Order book view =" << std::endl;
				print(Limit_Order_Book.first);
				return 1;
			}

			if (ordType == LIMIT && (Current_Bids_Depth == 0 || bestBid < ordPrice)) {						// Nothing in the LIMIT book to match against so match against market orders

				auto res = Sweep_And_Match_Market_Orders(ord);
				return res;
			}

			if ((ordType == LIMIT && ordPrice <= bestBid && ordQty > Top_Of_Bids_Book_Qty) ||
				(ordType == MARKET && ordQty > Top_Of_Bids_Book_Qty && Top_Of_Bids_Book_Qty != 0)) {

				return handle_allocations(ord, bestBid, Top_Of_Bids_Book_Qty, ordQty, ordType, ordPrice, ordID);

			}

			else { std::cout << "Control flow not caught" << std::endl; return 0; }

		}
		}
	}

	int handle_allocations(const Order ord, float bestBid, int Top_Of_Bids_Book_Qty, int ordQty, int ordType, float ordPrice, int ordID) {


		auto tally = Top_Of_Bids_Book_Qty;									// Tally of liquidity consumed by incoming order

		Allocation_Vec_Sells[0].first = bestBid;							// Vector of matched orders from the order book
		Allocation_Vec_Sells[0].second = Top_Of_Bids_Book_Qty;

		int residualVol{ 0 };												// Any liquidity left resting on the last touched order after sweeping through book
		int residualVolIndex{ 0 };
		int partialQty{ 0 };												// Final partial exec in the sweep
		int leavesQty{ ordQty - tally };									// Any leftover liquidity in the incoming order that cannot be matched
		int fullExecs{ 1 };													// Number of full executions in the sweep

		for (int i = 1; i < Max_Order_Book_Size; i++) {						// Sweep through order book until we have obtained the liquidity required


			if (Current_Bids_Depth - 1 - i < 0) {
				BOOST_LOG_TRIVIAL(info) << "Matching Engine: BUY side Order book exhausted " << std::endl;

				if (leavesQty) {										// If a partial order from original order remaining after sweeping, load into the order book
					Order newOrd = ord;
					newOrd.setQty(leavesQty);

					if (!Market_Orders_Buys_Vec.empty() && ordType == LIMIT) {
						Sweep_And_Match_Market_Orders(newOrd);
					}
					else
						Add_Order_to_Book(newOrd, true);
				}
				break;
			}

			Allocation_Vec_Sells[i].first = Limit_Order_Book.first[Current_Bids_Depth - 1 - i].getPrice();		// Build up the allocations vector from the main orderbook

			if ((Allocation_Vec_Sells[i].first >= ordPrice) || (ordType == MARKET)) {							// If the price is sufficient to meet the LIMIT order

				Allocation_Vec_Sells[i].second = Limit_Order_Book.first[Current_Bids_Depth - 1 - i].getQty();		// Add in the quantity at that level
				leavesQty = std::max(0, leavesQty - Allocation_Vec_Sells[i].second);
				auto newTally = tally + Allocation_Vec_Sells[i].second;

				if (newTally <= ordQty) fullExecs++;

				else {															// (newTally > ordQty)
					partialQty = ordQty - tally;								// Need to handle last partial allocate
					residualVolIndex = i;										// Track the index into the book
					residualVol = Allocation_Vec_Sells[i].second - partialQty;	// Calculate leftover volume to update that order in the book
					Allocation_Vec_Sells[i].second = partialQty;
					break;
				}
				tally = newTally;
			}

			else Allocation_Vec_Sells[i].first = 0;								// Bid price not high enough for LIMIT so zero out that price
		}

		for (int j = 0; j < Allocation_Vec_Sells.size(); j++) {	
																				// Execution reports for each partial exec 
			if (Allocation_Vec_Sells[j].second) {

				Execute_Trade(ordID, Allocation_Vec_Sells[j].first, Allocation_Vec_Sells[j].second);		// Could add a parent trade notification for the overall execution
				Allocation_Vec_Sells[j].second = 0;
			}
			else break;
		}


		BOOST_LOG_TRIVIAL(info) << "Matching Engine: removing from BUY order book - " << fullExecs << " full execs" << std::endl;

		Remove_From_Limit_Order_Book(BUY, fullExecs, residualVol, residualVolIndex);

		return 1;

	}



	 int  Sweep_And_Match_Market_Orders(const Order & ord) {					// Used when no LIMIT orders in the book , but pending market orders available for an incoming limit order to match against
		 
		 BOOST_LOG_TRIVIAL(info) << "Matching Engine: running Sweep_And_Match_Market_Orders " << std::endl;

		 auto ordPrice = ord.getPrice();
		 auto side = ord.getSide();
		 auto ordQty = ord.getQty();										
		 auto ordID = ord.getID();

		 auto tally{ 0 };													// Tally of market order liquidity consumed by incoming LIMIT order
		 int residualVol{ 0 };												// Any liquidity left resting on the last touched order after sweeping through book
		 int residualVolIndex{ 0 };
		 int partialQty{ 0 };
		 int leavesQty{ ordQty };
		 int fullExecs{ 0 };

		 switch (side) {
		 case SELL: {

			 if (size(Market_Orders_Buys_Vec) == 0) {  return 0; }				// No market orders to match with so return 0 indicating we just add the order to the book

				for (int i = 0; i < size(Market_Orders_Buys_Vec); i++) {				// Sweep through order book from second top-of-book order until we have obtained the liquidity required


														//  Vector to hold mappings of prices to quantity consumed as we traverse down the market order book

						BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Allocating new LIMIT order against possibly multiple market orders  " << i << std::endl;

						Market_Orders_Allocation_Vec_Buys[i].first = ordPrice;
						if (ordQty - tally <= Market_Orders_Buys_Vec[i].getQty()) {				// Remaining order amount covered by i-th market order

							Market_Orders_Allocation_Vec_Buys[i].second = ordQty - tally;
							residualVol = Market_Orders_Buys_Vec[i].getQty() - (ordQty-tally);
							residualVolIndex = i;
							break;
						}

						else {

							Market_Orders_Allocation_Vec_Buys[i].second = Market_Orders_Buys_Vec[i].getQty();

							leavesQty = std::max(0, leavesQty - Market_Orders_Allocation_Vec_Buys[i].second);
							if ((i == size(Market_Orders_Buys_Vec) - 1) && leavesQty) {

								BOOST_LOG_TRIVIAL(info) << "Matching Engine: Market orders SELLs side exhausted " << std::endl;
																						// If a partial order remaining, load into the order book
								Order newOrd = ord;
								newOrd.setQty(leavesQty);
								Add_Order_to_Book(newOrd, true);
								BOOST_LOG_TRIVIAL(info) << "Matching Engine:  New order entered with remaining Qty  " << leavesQty << std::endl;

							}
							BOOST_LOG_TRIVIAL(info) << "Matching Engine:  leavesQty  " << leavesQty << std::endl;
							auto newTally = tally + Market_Orders_Allocation_Vec_Buys[i].second;
							//std::cout << "\nnewTally " << newTally << std::endl;
							if (newTally <= ordQty) fullExecs++;

							else {															// (newTally > ordQty)
								partialQty = ordQty - tally;								// Need to handle last partial allocate
								residualVolIndex = i;										// Track the index into the book
								residualVol = Allocation_Vec_Buys[i].second - partialQty;		// Calculate leftover volume to update that order in the book
								Allocation_Vec_Buys[i].second = partialQty;
								break;
							}
							tally = newTally;
						}
				}
				for (int j = 0; j < Market_Orders_Allocation_Vec_Buys.size(); j++) {							// Send an exec report for each partial exec

					if (Market_Orders_Allocation_Vec_Buys[j].second) Execute_Trade(ordID, Market_Orders_Allocation_Vec_Buys[j].first, Market_Orders_Allocation_Vec_Buys[j].second);		// Could add a parent trade notification for the overall execution
				}

				BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Removing " << fullExecs << " from BUYS market order book " << std::endl;
				Remove_From_Market_Order_Vec(BUY, fullExecs);

				BOOST_LOG_TRIVIAL(info) << "Matching Engine: Residual volume to be updated on last order in book " << residualVol << " Residual vol index " << residualVolIndex << std::endl;

				if (residualVol) Market_Orders_Buys_Vec[residualVolIndex- fullExecs].setQty(residualVol);

				BOOST_LOG_TRIVIAL(info) << "Market_Orders_Buys_Vec after deletions " << std::endl;
				print(Market_Orders_Buys_Vec);
				return 1;
		 }

		 case BUY:{

			 //  Vector to hold mappings of prices to quantity consumed as we traverse down the market order book

			 if (size(Market_Orders_Sells_Vec) == 0) { return 0; } 			// No market orders to match with so return 0 indicating we just add the order to the book

			 for (int i = 0; i < size(Market_Orders_Sells_Vec); i++) {				// Sweep through order book from second top-of-book order until we have obtained the liquidity required



				 BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Allocating new LIMIT order against possibly multiple market orders  " << i << std::endl;

				 Market_Orders_Allocation_Vec_Sells[i].first = ordPrice;
				 if (ordQty - tally <= Market_Orders_Sells_Vec[i].getQty()) {				// Remaining order amount covered by i-th market order
					 Market_Orders_Allocation_Vec_Sells[i].second = ordQty - tally;
					 residualVol = Market_Orders_Sells_Vec[i].getQty() - (ordQty - tally);
					 residualVolIndex = i;
					 break;
				 }
				 else {
					 Market_Orders_Allocation_Vec_Sells[i].second = Market_Orders_Sells_Vec[i].getQty();
					 leavesQty = std::max(0, leavesQty - Market_Orders_Allocation_Vec_Sells[i].second);
					 if ((i == size(Market_Orders_Sells_Vec) - 1) && leavesQty) {
						 BOOST_LOG_TRIVIAL(info) << "Matching Engine: Market orders BUYS side exhausted " << std::endl;
						 // If a partial order remaining, load into the order book
						 Order newOrd = ord;
						 newOrd.setQty(leavesQty);
						 Add_Order_to_Book(newOrd, true);
						 BOOST_LOG_TRIVIAL(info) << "Matching Engine:  New order entered with remaining Qty  " << leavesQty << std::endl;

					 }
					 BOOST_LOG_TRIVIAL(info) << "Matching Engine:  leaves Qty  " << leavesQty << std::endl;
					 auto newTally = tally + Market_Orders_Allocation_Vec_Sells[i].second;
					 //std::cout << "\nnewTally " << newTally << std::endl;
					 if (newTally <= ordQty) fullExecs++;
					 else {															// (newTally > ordQty)
						 partialQty = ordQty - tally;								// Need to handle last partial allocate
						 residualVolIndex = i;										// Track the index into the book
						 residualVol = Market_Orders_Allocation_Vec_Sells[i].second - partialQty;		// Calculate leftover volume to update that order in the book
						 Market_Orders_Allocation_Vec_Sells[i].second = partialQty;
						 break;
					 }
					 tally = newTally;
				 }

			 }
			 for (int j = 0; j < Market_Orders_Allocation_Vec_Sells.size(); j++) {							// Send an exec report for each partial exec

				 if (Market_Orders_Allocation_Vec_Sells[j].second) Execute_Trade(ordID, Market_Orders_Allocation_Vec_Sells[j].first, Market_Orders_Allocation_Vec_Sells[j].second);		// Could add a parent trade notification for the overall execution
			 }

			 BOOST_LOG_TRIVIAL(info) << "Matching Engine:  Removing " << fullExecs << " from SELLS market order book " << std::endl;
			 Remove_From_Market_Order_Vec(SELL, fullExecs);
			 BOOST_LOG_TRIVIAL(info) << "Matching Engine: Residual volume to be updated on last SELL market order in book " << residualVol << " Residual vol index " << residualVolIndex << std::endl;
			 if (residualVol) Market_Orders_Sells_Vec[residualVolIndex-fullExecs].setQty(residualVol);			//Need to adjust index for deleted orders done in Remove_From_Market_Order_Vec(SELL, fullExecs)
			 
			 BOOST_LOG_TRIVIAL(info) << "Market_Orders_Sells_Vec after deletions " <<std::endl;
			 print(Market_Orders_Sells_Vec);
			 return 1;
		 }

		 }

	 }

	 void Remove_From_Market_Order_Vec(int side, int count) {

		 if (side == BUY) {

			 for (int i = 0; i < count; i++) {

				 if (size(Market_Orders_Buys_Vec) == 0) { BOOST_LOG_TRIVIAL(info) << "Matching Engine: All executed market orders removed from book"; return; }
				 BOOST_LOG_TRIVIAL(info) << "Matching Engine: Resetting order " << i << " ";
				 Market_Orders_Buys_Vec.erase(begin(Market_Orders_Buys_Vec) + i);					// Erase executed order
			 }
		 }
		 else {													//SELL

			 for (int i = 0; i < count; i++) {

				 if (size(Market_Orders_Sells_Vec) == 0) { BOOST_LOG_TRIVIAL(info) << "Matching Engine:  All executed market orders removed from book"; return; }
				 BOOST_LOG_TRIVIAL(info) << "Matching Engine: Resetting order " << i << " ";
				 Market_Orders_Sells_Vec.erase(begin(Market_Orders_Sells_Vec) + i);					// Erase executed order
			 }

		 }

	 }

	void Remove_From_Limit_Order_Book(int side, int count,int residualVol, int residualVolIndex) {

		if (side == BUY) {

			for (int i = 0; i < count; i++) {
				if (Current_Bids_Depth - 1 - i < 0) { BOOST_LOG_TRIVIAL(info) << "Matching Engine: Error index < 0 ";  }
				BOOST_LOG_TRIVIAL(info) << "Matching Engine: Resetting order "  << i << " " << Current_Bids_Depth << " " << Current_Bids_Depth - 1 - i;
				Limit_Order_Book.first[Current_Bids_Depth - 1 - i].reset();					// Zero out all members								
			}

			BOOST_LOG_TRIVIAL(info) << "Matching Engine: Residual volume to be updated on last touched order in bids book " << residualVol << " Residual vol index " << residualVolIndex << std::endl;
			if (residualVol) Limit_Order_Book.first[Current_Bids_Depth - residualVolIndex - 1].setQty(residualVol);

			BOOST_LOG_TRIVIAL(info) << "Matching Engine: Bids Limit Order book view = " << std::endl;
			print(Limit_Order_Book.first);
			Current_Bids_Depth -= count;

			BOOST_LOG_TRIVIAL(info) << "Matching Engine: All executed BUY orders removed from book"; 

			return;
		}

		else {

			for (int i = 0; i < count; i++) {

				if (Current_Asks_Depth - 1 - i < 0) { BOOST_LOG_TRIVIAL(info) << "Matching Engine : Error index < 0 ";  }
				BOOST_LOG_TRIVIAL(info) << "Matching Engine: Resetting order " << i << " " << Current_Asks_Depth << " " << Current_Asks_Depth - 1 - i;
				Limit_Order_Book.second[Current_Asks_Depth - 1 - i].reset(); 					// Zero out all members		
			}

			BOOST_LOG_TRIVIAL(info) << "Matching Engine: Residual volume to be updated on last touched order in asks book " << residualVol << " Residual vol index " << residualVolIndex << std::endl;
			if (residualVol) Limit_Order_Book.second[Current_Asks_Depth - residualVolIndex - 1].setQty(residualVol);
			print(Limit_Order_Book.second);
			Current_Asks_Depth -= count;

			BOOST_LOG_TRIVIAL(info) << "Matching Engine: All executed SELL orders removed from book";

			return;
		}

	}
	void Execute_Trade(int id, float price, int quantity) {
	
		BOOST_LOG_TRIVIAL(info) << "Matching Engine: Order ID "<< id <<" Executed " << quantity << " at " << price << std::endl;

	};

	bool Order_Book_Full(side s) const {
		return (s == BUY && Current_Bids_Depth == Max_Order_Book_Size || s == SELL && Current_Asks_Depth == Max_Order_Book_Size);
	}

	void Publish_Top_Of_Book() {

		while (!Orders::Order_Book_In_Use.load() && Current_Asks_Depth > 0 && Current_Bids_Depth > 0) {

			Orders::Order_Book_In_Use = true;
			BOOST_LOG_TRIVIAL(info) << "Publishing top of BUYS book - Price " << Limit_Order_Book.first[Current_Bids_Depth - 1].getPrice() 
				<< " Qty " << Limit_Order_Book.first[Current_Bids_Depth - 1].getQty() << std::endl;
			BOOST_LOG_TRIVIAL(info) << "Current bids depth = " << Current_Bids_Depth << std::endl;

			BOOST_LOG_TRIVIAL(info) << "Publishing top of SELLS book - Price " << Limit_Order_Book.second[Current_Asks_Depth - 1].getPrice() 
				<< " Qty " << Limit_Order_Book.second[Current_Asks_Depth - 1].getQty() << std::endl;
			BOOST_LOG_TRIVIAL(info) << "Current asks depth = " << Current_Asks_Depth << std::endl;

			Orders::Order_Book_In_Use = false;
			std::this_thread::sleep_for(5s);
		}


	}

	int purge_GFD_Orders() {							// Function to call at midnight to purge GFD orders

		if (!Orders::Order_Book_In_Use.load()) {

			Orders::Order_Book_In_Use = true;

			std::remove_if(begin(Limit_Order_Book.first), end(Limit_Order_Book.first), [](auto elem) { return  (elem.get_GFD_Status()); });
			std::remove_if(begin(Limit_Order_Book.second), end(Limit_Order_Book.second), [](auto elem) { return (elem.get_GFD_Status()); });
		}
		else return 0;
		Orders::Order_Book_In_Use = false;

		return 1;

	}


private:

	inline static std::pair<std::vector<Order>, std::vector<Order>>  Limit_Order_Book;		// The buy and sell orderbook for the symbol
	int Max_Order_Book_Size{ 50 };

	inline static int Current_Bids_Depth{ 0 };
	inline static int Current_Asks_Depth{ 0 };

	std::string  myISIN;
	inline static std::vector<std::pair<float, int>> Allocation_Vec_Buys;						// Vector to hold mappings of prices to quantity consumed as we traverse down the Buy order book
	inline static std::vector<std::pair<float, int>> Allocation_Vec_Sells;  					// Vector to hold mappings of prices to quantity consumed as we traverse down the Sell order book
	enum orderType { MARKET, LIMIT };

	inline static std::vector<Order>   Market_Orders_Buys_Vec;							// Vector to hold market orders - Buy order book
	inline static std::vector<Order>   Market_Orders_Sells_Vec;							// Vector to hold market orders - Sell order book
	
	inline static std::vector<std::pair<float, int>>  Market_Orders_Allocation_Vec_Buys;
	inline static std::vector<std::pair<float, int>>  Market_Orders_Allocation_Vec_Sells;

};



class Matching_Engine {

public:

	Matching_Engine() {

		BOOST_LOG_TRIVIAL(info) << "Matching Engine: Constructing engine " << std::endl;
		t2 = std::thread(&Matching_Engine::Receive_Orders, this);

		for (auto & symbol : ISINs) {

			auto ob = orderBook1Symbol(symbol);
			Complete_Order_Book[symbol] = ob;

			//buyOrders.resize(Max_Order_Book_Size);
			//sellOrders.resize(Max_Order_Book_Size);

		}
		BOOST_LOG_TRIVIAL(info) << "Matching Engine: Engine Constructed " << std::endl;
	};



	void Receive_Orders() {

		BOOST_LOG_TRIVIAL(info) << "Matching Engine: Starting receive orders into Order book function now "  << std::endl;

		while (true) {

			if (Orders::ordersReady) {
			//std::cout << "ordersReady true " << std::endl;

			while (!Orders::orderBuffer.empty()) {

				auto ord = Orders::orderBuffer[size(Orders::orderBuffer) - 1];
				Orders::orderBuffer.pop_back();
				//std::cout << "Size of order buffer now = " << size(orderBuffer) << std::endl;
				auto sym = ord.getISIN();
				auto side = ord.getSide();
				ord.setArrivalTime();

				Complete_Order_Book[sym].Add_Order_to_Book(ord,false);
				//std::cout << "Size of order book now = " << sym <<  std::endl;
			}

			Orders::ordersReady = false;
			}

		}
	}

	void Publish_Top_Of_Book() {
		for (auto& sym : ISINs) {
			Complete_Order_Book[sym].Publish_Top_Of_Book();
		}
	}


	//void fixDecoder(std::string FIXString) {
	//	std::istringstream iss(FIXString);
	//	FIXMessage fm;
	//	std::string tag;
	//	while (iss >> tag) {
	//		auto sTagID = tag.substr(0, tag.find("=")-1);
	//		auto sTagValue = tag.substr(tag.find("=")+1);
	//		auto tagID = std::stoi(sTagID);

	//		switch (tagID) {
	//		case 35:  fm.MessageType = sTagValue;
	//		case 49:  fm.senderCompID = sTagValue;
	//		}


	//	}
	//}

private:

	int numSupportedSecurities{ 5 };


	std::vector<Order>  buyOrders;
	std::vector<Order>  sellOrders;

	//std::pair<std::vector<Order>, std::vector<Order>>   orderBook1Symbol;

	std::unordered_map< std::string, orderBook1Symbol>  Complete_Order_Book;

	std::thread t2;

	std::array<std::string, 5> ISINs{ "GB0007980591"s , "GB00B03MLX29"s, "DE0005190003"s, "ES0178430E18"s, "FR0000131104"s};




};

class FIXMessage {
public:
	std::variant<int, std::string> MessageType;
	std::string senderCompID;
	std::string targetCompID;



};






#endif
