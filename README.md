# SimpleFTP

FTSERVER.C
To compile:
gcc -o ftserver ftserver.c

To run:
ftserver [serverPort#]

To quit: 
Ctrl+C


FTCLIENT.PY
To run:
python3 ftclient.py [hostname] [serverPort#] -l [dataPort#]

OR (to request a file):
python3 ftclient.py [hostname] [serverPort#] -g [filename] [dataPort#]


Example:
(Server side)
ftserver 40000

(Client side)
python3 ftclient.py flip 40000 -g file.txt 40001