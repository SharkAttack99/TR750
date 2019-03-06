LOCAL_SRC_FILES:= main.c expression-parser.c gauge.c interface-linux-i2c.c gauge-simulator.c
LOCAL_MODULE := bqtool
LDFLAGS += -lm

all:
	$(CC) $(CFLAGS) $(LOCAL_SRC_FILES) -o $(LOCAL_MODULE) $(LDFLAGS)

clean:
	rm -f $(LOCAL_MODULE) 
