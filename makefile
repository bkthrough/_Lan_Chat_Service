all:ser

ser:ser.cpp
	g++ ser.cpp -o ser -lcrypt -lmysqlclient -L/usr/lib64/mysql -lpthread -g

.PHONY:clean

clean:
	rm -f ser
