OBJ_DIR:= ./obj
COMPILE:=arm-linux-gnueabihf-
SRC:=Crc.c Common.c td_func.c tda_func.c



all:$(SRC)
	$(COMPILE)gcc -Wall -Wextra -fno-stack-protector -g -o macLoader -pthread $(SRC) Main.c -lrt -lm


.PHONY : clean
clean:
	rm -rf macLoader $(OBJ_DIR)/*.o 
