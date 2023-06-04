# CS118 Project 2

This is the repo for spring23 cs118 project 2.
The Docker environment has the same setting with project 0.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.

## Provided Files

- `project` is the folder to develop codes for future projects.
- `grader` contains an autograder for you to test your program.
- `scenarios` contains test cases and inputs to your program.
- `docker-compose.yaml` and `Dockerfile` are files configuring the containers.

## Docker bash commands

```bash
# Setup the container(s) (make setup)
docker compose up -d

# Bash into the container (make shell)
docker compose exec node1 bash

# Remove container(s) and the Docker image (make clean)
docker compose down -v --rmi all --remove-orphans
```

## Environment

- OS: ubuntu 22.04
- IP: 192.168.10.225. NOT accessible from the host machine.
- Files in this repo are in the `/project` folder. That means, `server.cpp` is `/project/project/server.cpp` in the container.
  - When submission, `server.cpp` should be `project/server.cpp` in the `.zip` file.

## Project 2 specific

### How to use the test script

To test your program with the provided checker, go to the root folder of the repo and
run `python3 grader/executor.py <path-to-server> <path-to-scenario-file>`.  
For example, to run the first given test case, run the following command:
```bash
python3 grader/executor.py project/server scenarios/setting1.json
# Passed check point 1
# Passed check point 2
# OK
```

If your program passes the test, the last line of output will be `OK`.
Otherwise, the first unexpect/missing packet will be printed in hex.
Your program's output to `stdout` and `stderr` will be saved to `stdout.txt` and `stderr.txt`, respectively.
You can use these log files to help you debug your router implementation.
You can also read `executor.py` and modify it (like add extra outputs) to help you.
We will not use the grader in your submitted repo for grading.

### How to write a test scenario

A test scenario is written in a JSON file. There are 5 example test cases in the `scenarios` folder.
The fields of the JSON file are:

- `$schema`: Specify the JSON schema file so your text editor can help you validate the format.
  Should always point to `setting_schema.json`.
- `input`: Specify the input file to the program. Should use relative path to the JSON file.
- `actions`: A list of actions taken in the test scenario. There are 3 types of actions:
  - `send`: Send a TCP/UDP packet at a specified port (`port`).
  - `expect`: Expect to receive a TCP/UDP packet at a specified port (`port`).
  - `check`: Delay for some time for your server to process (`delay`, in seconds).
    Then, check if all expectations are satisfied.
    All packets received since the last checkpoint must be exactly the same as specified in `expect` instructions.
    There should be no unexpected or missing packets
  - The last action of `actions` must be `check`.
- The fields of a packet include:
  - `port`: The ID of the router port to send/receive the packet, not the port number.
  The port numbers are specified in `src_port` and `dst_port`.
  - `src_ip` and `src_port`: The source IP address and port number.
  - `dst_ip` and `dst_port`: The destination IP address and port number.
  - `proto`: The transport layer protocol. Can only be `tcp` or `udp`.
  - `payload`: The application layer payload of the packet. Must be a string.
  - `ttl`: Hop limit of the packet.
  - `seq`: TCP sequence number.
  - `ack`: TCP acknowledge number.
  - `flag`: The flag field in TCP header. Should be specified in numbers. For example, ACK should be `16`.
  - `rwnd`: TCP flow control window.
  - `ip_options_b64`: The IP options. Must be encoded in base64 if specified.
  - `ip_checksum`: The checksum for an IP packet. Automatically computed to be the correct number if not specified.
  - `trans_checksum`: The checksum in the TCP/UDP header. Automatically computed to be the correct number if not specified.
  - Most of these fields are optional, but omitting mandatory fields may crash the grader.

Please read the example JSON files and the schema JSON for details.

### How to examine a test scenario

To print all packets in a test scenario in hex format,
run `python3 grader/packet_generate.py` and input the JSON setting.
You may also use `<` to redirect the input to the JSON file, like
```bash
python3 grader/packet_generate.py < scenarios/setting1.json
# ================== SEND @@ 01 ==================
# 45 00 00 1c 00 00 40 00  40 11 b6 54 c0 a8 01 64 
# c0 a8 01 c8 13 88 17 70  00 08 50 69
# ================== ========== ==================
#
# ================== RECV @@ 02 ==================
# 45 00 00 1c 00 00 40 00  3f 11 b7 54 c0 a8 01 64 
# c0 a8 01 c8 13 88 17 70  00 08 50 69
# ================== ========== ==================
#
# Check point 1
#
# ================== SEND @@ 01 ==================
# 46 00 00 20 00 00 40 00  40 11 b4 4f c0 a8 01 64 
# c0 a8 01 c8 01 01 00 00  13 88 17 70 00 08 50 69
# ================== ========== ==================
#
# ================== RECV @@ 02 ==================
# 46 00 00 20 00 00 40 00  3f 11 b5 4f c0 a8 01 64 
# c0 a8 01 c8 01 01 00 00  13 88 17 70 00 08 50 69
# ================== ========== ==================
#
# Check point 2
#
```

### Other notes

- We will use a different version of grader for the final test to integrate with Gradescope.
  But it will be similar to the given one.
  Modifying the grader in this repo will not affect anything.
- We will include many hidden test cases in the final test. Do not fully depend on the 5 given ones.
  They do not cover all edge cases that we want to test.
- The autograder will only build your program in the `project` folder, and grade the built `server` executable.
  Your program should not depend on other files to run.

## TODO

Project Members:
Justin Hu 205-514-102
Randy Gu 305-592-076

High level Design:

We divided the project up into 2 main sections: Processing a single IP packet, and everything else. Justin worked in
main.cpp doing connection and map setups, dealing with sockets and select(), and placing a whole IP packet into
one continuous char buffer. Randy worked in functions.cpp doing packet processing logic such as checksum calculations,
drop condition checks, and address rewrites. In main, the entire IP packet is processed in buffer with one call to the 
IP processing function and then sent out to the listed destination or dropped.

Major problems:

We ran into two main problems, packets not being sent and checksum problems. Checksums are very annoying to deal with
and issues were usually due to small details about the computation that we missed such as TCP padding. We also had
a strange issue with packets not being sent out in time because the sockets were being closed, which seems to imply
that our program was taking too long to run. Initially, we wrote the select() code as to be able to handle multiple
IP packets at once as well as partial IP packets. In an act of desperation after hours of no progress, we deleted 
everything but the most basic code to only handle single, whole IP packets and our code magically started to work. 
We still have no explanation as to why this is the case, but we suspect that reading in 0 bytes from the socket was
causing it to close prematurely, and so it turns out that the dumber, simpler way of assuming one packet per select 
was the right way to go.

References:

In learning how to use select(), we heavily referenced TA Boyan Ding's example code at 
https://github.com/dboyan/CS118-S23-1A/blob/main/Week%207/select.c as well as the Beej guide 
https://beej.us/guide/bgnet/html/split/slightly-advanced-techniques.html#select

Another student in discussion section 1D suggested the use of unordered maps to store address socket translations as 
well as the NAT table, which we found to be a good solution.

RFC 791 Internet Protocol
RFC 768 User Datagram Protocol
RFC 9293 Transmission Control Protocol 

For details about headers and checksum computations.
