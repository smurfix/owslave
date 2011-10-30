#!/usr/bin/python

# Usage: gen_eeprom FAMILY [48bit]
# 
# Prints an 8-byte string (family code, 6-by random ID, checksum).

import sys

id = [int(sys.argv[1],16)]
if len(sys.argv) == 3:
	s = sys.argv[2]
	if len(s) != 12:
		assert RuntimeError("random ID must be 6 bytes long!")
	while s:
		id.append(int(s[0:2],16))
		s = s[2:]
else:
	rand = open("/dev/urandom","r")
	s = rand.read(4)
	rand.close()
	id.extend((ord(c) for c in s))
	id.extend((0xBE,0x42));

table=( 0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
	157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
	35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
	190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
	70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
	219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
	101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
	248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
	140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
	17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
	175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
	50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
	202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
	87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
	233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
	116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53,
    )

crc=0
for i in id:
	crc = table[crc ^ i]
id.append(crc)

# Byte count, two hex digits, a number of bytes (hex digit pairs) in the data field. 16 (0x10) or 32 (0x20) bytes of data are the usual compromise values between line length and address overhead.
# Address, four hex digits, a 16-bit address of the beginning of the memory position for the data. Limited to 64 kilobytes, the limit is worked around by specifying higher bits via additional record types. This address is big endian.
# Record type, two hex digits, 00 to 05, defining the type of the data field.
# Data, a sequence of n bytes of the data themselves, represented by 2n hex digits.
# Checksum, two hex digits - the least significant byte of the two's complement sum of the values of all fields except fields 1 and 6 (Start code ":" byte and two 

sum=0
for i in (8,0,0,0)+tuple(id):
	sum += i
sum = (0x100-sum)&0xFF
print ":%02X%04X%02X%s%02X" % (8,0,0,"".join("%02X"%i for i in id),sum)
print ":00000001FF"
