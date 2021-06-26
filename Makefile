CXXFLAGS = -I include/ -O2 -g -Wall -fmessage-length=0 -std=c++11 -pthread

SRC_DIR_LIB=src
SRC_DIR_EXE=main
OBJ_DIR_LIB=obj/lib
OBJ_DIR_EXE=obj/exe
BIN_DIR=bin
HEAD_DIR=include

SRC_FILES_LIB = $(wildcard $(SRC_DIR_LIB)/*.cpp)
SRC_FILES_EXE = $(wildcard $(SRC_DIR_EXE)/*.cpp)
HEAD_FILES    = $(wildcard $(HEAD_DIR)/*.h)

OBJ_FILES_LIB = $(patsubst $(SRC_DIR_LIB)/%.cpp,$(OBJ_DIR_LIB)/%.o,$(SRC_FILES_LIB))
OBJ_FILES_EXE = $(patsubst $(SRC_DIR_EXE)/%.cpp,$(OBJ_DIR_EXE)/%.o,$(SRC_FILES_EXE))

EXEC_FILES  = $(patsubst $(SRC_DIR_EXE)/%.cpp,$(BIN_DIR)/%,$(SRC_FILES_EXE))

$(OBJ_DIR_EXE)/%.o:	$(SRC_DIR_EXE)/%.cpp $(OBJ_FILES_LIB) $(HEAD_FILES)
	$(CXX) -o $@ -c $< $(CXXFLAGS)

$(OBJ_DIR_LIB)/%.o:	$(SRC_DIR_LIB)/%.cpp $(HEAD_FILES)
	$(CXX) -o $@ -c $< $(CXXFLAGS)
	
$(BIN_DIR)/%:	$(OBJ_DIR_EXE)/%.o
	$(CXX) -o $@ $(subst $(BIN_DIR)/,$(OBJ_DIR_EXE)/,$@).o $(OBJ_FILES_LIB) $(HEAD_FILES) $(LDFLAGS) $(CXXFLAGS)

all:	$(EXEC_FILES) $(OBJ_FILES_LIB)
	@echo "Cleaning and Symlinking."
	rm -f obj/lib/*.o
	rm -f obj/exe/*.o
	ssh 192.168.1.31 'rm -f /home/smdupor/Server'
	ssh 192.168.1.32 'rm -f /home/smdupor/Server'
	scp bin/Server 192.168.1.31:/home/smdupor/
	scp bin/Client 192.168.1.31:/home/smdupor/
	scp bin/Server 192.168.1.32:/home/smdupor/
	#ln -sf ./bin/Client Client
	#Note Bindir used to say :	$(CXX) -o $@ $(subst $(BIN_DIR)/,$(OBJ_DIR_EXE)/,$@).o $(OBJ_FILES_LIB) $(HEAD_FILES) $(LDFLAGS) $(CXXFLAGS)
	#Note CXX flags used to say: CXXFLAGS = -I include/ -O2 -g -Wall -fmessage-length=0 -std=c++11 -pthread
	#ln -sf ./bin/RegServ RegServ
	@echo "****************************************************************************"
	@echo "************************ BUILD COMPLETE ************************************"
	@echo "****************************************************************************"

show:
	@echo "SRC_FILES_LIB=$(SRC_FILES_LIB)"
	@echo "HEADERS=$(HEAD_FILES)"
	@echo "SRC_FILES_EXE=$(SRC_FILES_EXE)"
	@echo "$(EXEC_FILES)"

clean:
	rm -rf *.o
	rm -f ./bin/*
