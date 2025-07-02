# Reliable-File-Transfer-over-UDP-using-custom-protocol
This project implements a simple yet reliable file transfer system over the UDP protocol. UDP by nature is fast but unreliable — it doesn't guarantee delivery, order, or integrity. This system builds a custom protocol on top of UDP to ensure reliable, ordered, and complete file transfer between two peers using batching, part tracking, timeouts, and retransmission logic.

---

## 📦 Features

- Custom protocol messages: `SENDING`, `SENDBATCH`, `DATA`, `RESENDPARTS`
- File split into batches and parts for scalable transfer
- Bitmap-based tracking of received parts
- Automatic retransmission of lost or delayed packets
- Adaptive part size negotiation between sender and receiver
- Timers for reliable retry handling
- Command-line interface for sending and receiving files

---

## 🗂️ Project Structure

├── main.c # Entry point; command parsing and socket setup
├── sender.c # Handles sending file parts and retransmissions
├── receiver.c # Handles receiving file parts and managing requests
├── timer.c # Provides timeout and timer utilities
├── part.c # Manages bitmap data structure to track parts
├── helper.c # Utility functions (file I/O, byte ops, etc.)
├── Makefile # Compilation script
├── intr/ # Header files and protocol constants
│ ├── consts.h
│ ├── helper.h
│ ├── main.h
│ ├── ops.h
│ ├── part.h
│ ├── receiver.h
│ ├── sender.h
│ └── timer.h
└── README.md


---

## 🔧 How to Build

### Prerequisites

- GCC or any modern C compiler
- Linux environment (uses `clock_gettime`, `arpa/inet`, etc.)

### Compilation

Use the included `Makefile` for compilation:

```bash
make

🕹️ How to Use

On Receiver Side
./file_transfer <your_port> <sender_port>
Then, type:
receive
On Sender Side (in another terminal or machine)
./file_transfer <your_port> <receiver_port>
Then, type:
send <filename>

The system automatically:
-Splits the file into parts
-Sends them in batches
-Retransmits any lost parts
-Reconstructs the file on the receiving end

✅ Received files are saved in the recv/ directory.

💡 Protocol Design
Message Types
Message Type	     Description
SENDING	File       Metadata and part size info
SENDBATCH	         Request for a batch of parts
DATA	             A specific part of a batch
RESENDPARTS	       Request to resend missing parts

File Splitting Strategy
-File is split into batches
-Each batch consists of multiple fixed-size parts (partsize)
-Each part is tracked using a bitmap
-If a batch is incomplete, the receiver sends a RESENDPARTS request

📂 Example
On Terminal 1 (Receiver):

Copy
Edit
./file_transfer 5001 5000
receive
On Terminal 2 (Sender):

Copy
Edit
./file_transfer 5000 5001
send myfile.txt

🛡️ Reliability Features
-Timeout and exponential backoff on message retries
-Bitmap-based tracking of received parts
-Retransmission of only missing parts
-Sender adapts partsize based on receiver capability

🙏 Acknowledgments
Built to understand and explore:
-Network reliability
-Custom protocol design
-UDP challenges and strategies for mitigation
