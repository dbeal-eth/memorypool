
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <vector>
#include <map>

#include "wallet.h"

#include "blockpayment.h"

using boost::asio::ip::tcp;

extern boost::shared_mutex poolQueueLock;
extern BlockPayment::Queue poolQueue;

extern std::map<std::string, BlockPayment::Nodes::User> userInfos;

extern float sharerate;

extern const int PAYOUT_DELAY;

class CPoolClient {
	public:
		CPoolClient(CWallet* pwallet, bool xpt) {
			wallet = pwallet;
			extraNonce = 0;
			stop = false;
			//this->xpt = xpt;
		}
		virtual ~CPoolClient() {
			
		}
		
		void Run(boost::shared_ptr<tcp::socket> sock);
		void HandleSubmission(const boost::system::error_code& error);
		void Sendwork();
		
		//boost::thread *mainThread;
		bool stop;
		
		static boost::shared_mutex clientsLock;
		static std::vector<CPoolClient *> clients;
	private:
	
		//bool xpt; <- no XPT in this currency
		
		std::vector<CBlock> submitted;
		
		struct KeyBlockPair {
			KeyBlockPair(CWallet *wallet) : key(wallet) {}
			CReserveKey key;
			CBlock block;
		};
		
		std::vector<KeyBlockPair> keys;
		CWallet* wallet;
		
		unsigned int extraNonce;
		
		std::string address;
		unsigned char majorVersion, minorVersion, payload, fee;
		unsigned short minerId;
		unsigned int sieveExtensions, sievePercentage, sieveSize;
		unsigned char buf[88]; // 128 was size of primecoin block
		boost::shared_ptr<tcp::socket> sock;
};

/*void PoolClientThread(boost::shared_ptr<tcp::socket> socket);

class CPoolServer {
	public:
		CPoolServer(short port);
	private:
		
		void Accept();
		void HandleAccept(boost::shared_ptr<tcp::socket> socket, const boost::system::error_code& error);
		
		boost::asio::io_service io;
		tcp::acceptor acceptor;
};

extern CPoolServer *gPoolServer;*/

void StartPool(boost::thread_group& threadGroup, CWallet* pwallet);

