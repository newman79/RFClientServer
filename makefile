CXXFLAGS=-Wwrite-strings -Wformat -Wparentheses

all:  RFSocketClient RFSocketServer

RFSocketClient: RFSocketClientMain.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $+ -o $@ 

RFSocketServer: RFSocketServerMain.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $+ -o $@ 
	
clean:
	$(RM) *.o  RFSocketClient RFSocketServer
