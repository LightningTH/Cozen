import sys, os
data = open("chars.txt", "r").read().split("\n\n")

f = open("chars.h", "w")
for c in range(0, len(data)):
	values = "{"
	lines = data[c].split("\n")
	comment = lines.pop(0)
	for l in lines:
		values += "0x%02x," % (int(l, 2))

	values = values[:-1] + "}"
	if(c != (len(data) - 1)):
		values += ","
	values += " " + comment + "\n"

	f.write(values)

f.close()
