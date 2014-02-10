//#include "Nodes.h"
#include <cstdlib>
#include <vector>
#include <string>
#include <map>

//for debug only
#include <iostream>

namespace BlockPayment {
	const unsigned int SHARE_TARGET = 0x200fffff;
	
    //if you want to know why like this look at Block::calcShares
    const unsigned int MIN_CHAIN_LENGTH = 6; //lowest lenght to pay
    const unsigned int MAX_CHAIN_LENGTH = 10; //anything above this default to same as this
    //WARNING: that if one of the above things are off, there could be an out of bounds error while accessing the array
    const unsigned int CHAIN_VALUES[] = {1, 15, 170, 1900, 21000}; //value[0] = 6 chain value
    const double FEE = .02; //1 = 100%; .01 = 1%
    
    const double MIN_PAYOUT = 0.5;

    namespace Nodes {
        struct User;
        class Block {
            Block* next;
            User* h;

            unsigned int height;
            double XPM; //the XPM found for this block
            bool calculatedXPM;
            unsigned int runningtotal; //number of shares

            unsigned int calcShares(unsigned int chainlength);

        public:
            Block(unsigned int newheight);
            ~Block();
            std::vector<std::string> getUserIdList();
            User* getUser(std::string user);
            void addShares(std::string user, unsigned int chainlength);
            void calculateXPMPerUser();
            void calculateXPMPerUser(double newxpm);
            Block* getNext();
            unsigned int getHeight();
            double getXPM();
            unsigned int getRunningTotal();
            User* getUserList(); //best way to go through and find the XPM for each
            bool hasCalculatedXPM();
            void deleteNext();
            void setNext(Block* newnext);
            void setUser(User* in);
            void setHeight(unsigned int in);
            void setXPM(double newxpm);
            void setRunningTotal(unsigned int total);
            void save(std::string file);
            static Block* load(std::string file);
        };

        struct User {
            User* next;
            std::string username;
            unsigned int runningtotal;
            double XPM;

            User() {
                next = NULL;
                runningtotal = 0;
                XPM = 0;
            }
            User(std::string user, unsigned int shares) {
                next = NULL;
                XPM = 0;
                username = user;
                runningtotal = shares;
            }
            ~User() {
                if(next != NULL) delete next;
            }
        };
    }

    class Queue {
        Nodes::Block* h; //head, the one next needing to be cashed out (next) peek gets this
        Nodes::Block* r; //rear, where the new ones are added (current) getCurrent gets this
        std::map<std::string, double> unconfirusrbals; //unconfirmed user balances

    public:
        Queue();
        ~Queue();
        Nodes::Block* getCurrent();
        Nodes::Block* peek();
        Nodes::Block* deq();
        void newBlock(unsigned int newheight);
        void addShares(std::string user, unsigned int chainlenght); //will add them if they are not already there
        void setHeight(unsigned int in, bool current = true);
        void setXPM(double in, bool current = true);
        void calculateXPMPerUser(bool current = true); //use this if XPM already set
        void calculateXPMPerUser(double XPM, bool current = true); //sets XPM and then calculates
        void calculateAllXPM(); //remember the conditions under which it actually calculates
        void updateUnconfirmedUserBalances(); //this calls calculateAllXPM()
        double getUsersUnconfirmedBalance(std::string user, bool update = false); //set to tue and it will auto call updateUnconfirmedUserBalances()
        unsigned int getHeight(bool current = true); //assumes the current block
        double getXPM(bool current = true);
        Nodes::User* getUserList(bool current = true);
        bool hasCalculatedXPM(bool current = true);
        void save(std::string file, bool current = true); //if false it will save the head
        void load(std::string file);
        void loadNewBlock(std::string file); //enqs the new block (AKA this becomes the current one)
        void loadNewBlocks(std::vector<std::string> files); //does same as the one above but for multiple blocks

        //debug only
        inline void printAll() {
            Nodes::Block* a = h;
            while(a != NULL) {
                std::cout << a->getHeight() << ": XPM:" << a->getXPM() << ", Total Shares: " << a->getRunningTotal() << '\n';
                Nodes::User* b = a->getUserList();
                while(b != NULL) {
                    std::cout << "    " << b->username << ": " << b->runningtotal << "; " << b->XPM << '\n';
                    b = b->next;
                }
                a = a->getNext();
            }
        }
    };
}
