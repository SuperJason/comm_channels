Compile
==========
  `make`

Test commands
==========
* Command Line
  Step 1>
      # Terminal 1:
	`./channels`
      # Terminal 2:
	`./channels -c -a "127.0.0.1" -p 8000`
      # Terminal 3:
	`cat srv_receive_fifo`
      # Terminal 4:
	`echo abc > cli_send_fifo`

      # Terminal 3 receive "abc"
  Step 2>
      # Terminal 3:
	`cat cli_receive_fifo`
      # Terminal 4:
	`echo 123 > srv_send_fifo`

      # Terminal 3 receive "123"
  Step 3>
      # Terminal 4:
	`echo rx_quit > srv_send_fifo`
      # Terminal 1 `./channels` quit
      # Terminal 4:
	`echo rx_quit > cli_send_fifo`
      # Terminal 2 `./channels -c -a "127.0.0.1" -p 8000` quit

* Test script
  # Terminal 1:
    `./channels`
  # Terminal 2:
    `./channels -c -a "127.0.0.1" -p 8000`
  # Terminal 3:
    `python3 send_test.py`
