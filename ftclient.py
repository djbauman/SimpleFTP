# Daniel Bauman
# (ftclient.py)
# Program Description: This program connects to a listening server on a given port,
# then requests either the server's directory listing or a specific file. If the
# request is valid, it will receive this over a second user-specified port.



from socket import *
import sys
import signal
import os

BUFFER_SIZE = 501

def validationError():
	print("Type client.py [server name] [control port #] -l [data port #]")
	print("OR client.py [server name] [control port #] -g [filename] [data port #]")
	sys.exit(1)

def main():
	# Validate arguments
	if (len(sys.argv) != 5) and (len(sys.argv) != 6):
		validationError()

	if ((len(sys.argv) == 5) and (sys.argv[3] == "-g")) or ((len(sys.argv) == 6) and (sys.argv[3] == "-l")):
		validationError()

	# Set controlPort and command variables
	serverName = sys.argv[1]
	controlPort = int(sys.argv[2])
	command = sys.argv[3]

  # Set dataPort and filename variables
	if (len(sys.argv) == 5):
		dataPort = int(sys.argv[4])
	elif (len(sys.argv) == 6):
		filename = sys.argv[4]
		dataPort = int(sys.argv[5])

	# Establish control connection
	clientSocket = socket(AF_INET, SOCK_STREAM)
	clientSocket.connect((serverName,controlPort))
	# print("Connected to server on port " + str(controlPort) + " (control)")

	# Send "-l", "-g <filename>", or an invalid command to server
	if ((command == "-l") or (command != "-g")):
		msgOut = bytes(command, 'utf-8')	
	elif (command == "-g"):
		msgOut = command + " " + filename
		msgOut = bytes(msgOut, 'utf-8')
	clientSocket.send(msgOut)

	# Get response from server (either an error, or "OK")
	msgIn = clientSocket.recv(BUFFER_SIZE)
	msgIn = msgIn.decode('utf-8')

	# If response was not OK, display response
	if (msgIn != "OK"):
		print("Response from server: " + msgIn)

	# Otherwise, send dataPort to server...
	else:
		msgOut = bytes(str(dataPort), 'utf-8')
		clientSocket.send(msgOut)

		# ... and establish data connection
		clientSocket2 = socket(AF_INET, SOCK_STREAM)
		clientSocket2.connect((serverName, dataPort))
		# print("Connected to server on port " + str(dataPort) + " (data)")

		# Receive data from server
		# If -l was sent, receive and print directory
		if (command == "-l"):
			fileList = clientSocket2.recv(BUFFER_SIZE)
			fileList = fileList.decode('utf-8')
			print("\nDirectory received from " + serverName + ":")
			print(fileList)

		# If -g was sent, retrieve the requested file	
		elif (command == "-g"):
			# Receive either "File not found" or "OK" message from server
			msgIn = clientSocket.recv(BUFFER_SIZE)
			msgIn = msgIn.decode('utf-8')

			# If the server found the file, send the file
			if (msgIn == "File found"):
				# Handle duplicate filenames (TO IMPROVE: Handle file names without file extensions)
				files = os.listdir('.')
				x = 1
				while filename in files:
					if (x == 1):
						filename = filename.split(".")
						filename = filename[0] + "1." + filename[1]
						x = 2
					else:
						lastNum = str(x-1) + "."
						filename = filename.split(lastNum)
						filename = filename[0] + str(x) + "." + filename[1]
						x += 1

				file = open(filename, 'w')
				data = clientSocket2.recv(BUFFER_SIZE)
				data = data.decode('utf-8')
				while(data):
					file.write(data)
					data = clientSocket2.recv(BUFFER_SIZE)
					data = data.decode('utf-8')
				print("Transfer complete.")

			# Otherwise (if msgIn was "File not found")
			elif (msgIn == "File not found"):
				print(msgIn)


		clientSocket2.close()
		# print("Connection to server closed (data).")


	clientSocket.close()
	# print("Connection to server closed (control).")
	sys.exit(0)



if __name__ == "__main__":
	main()