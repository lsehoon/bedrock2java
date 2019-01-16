all: bed2java

bed2java: main.cpp
	g++ -o $@ $<  -lleveldb -lpthread -lsnappy -lz -DDLLX= 

clean:
	rm -f bed2java
