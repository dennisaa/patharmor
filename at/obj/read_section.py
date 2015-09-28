import os,sys
import struct

SECTION = dict();
SECTION[".data "] = 0;
SECTION[".rodata "] = 0;
SECTION[".dynsym "] = 0;
#SECTION[".rela.dyn "] = 0;
#SECTION[".rela.plt "] = 0;
_TEXT_START = 0;
_TEXT_END = 0;
_GOT_START = 0;
_GOT_END = 0;
_PLT_START = 0;
_PLT_END = 0;
_DATA_START = 0;
_DATA_END = 0;
PLT_ENTRY = dict();

def read_plt_entries():
	global PLT_ENTRY

	f = open("./plt.log","r");
	d = f.readlines();
	f.close();

	for l in d:
		addr = int(l.split(" ")[0],16);
		name = l.split("<")[1].split("@plt>:")[0];
		PLT_ENTRY[name] = addr;
	return;

def search_plt_entries(ename):
	global PLT_ENTRY

	strip_name = ename;
	if (ename.find(" ")!=-1):
		strip_name = ename.split(" ")[0];
	
	if strip_name in PLT_ENTRY.keys():
		return PLT_ENTRY[strip_name];
	
	return 0;


def read_section(sname, bname):
	global SECTION
	global _TEXT_START
	global _TEXT_END
	global _GOT_START
	global _GOT_END
	global _PLT_START
	global _PLT_END
	global _DATA_START
	global _DATA_END

	os.system("objcopy -O binary --only-section=%s ./%s tmp_log"%(sname,bname));
	with open("./tmp_log","rb") as f:
		d = f.read();
	for i in range(0,len(d)-8):
		val = struct.unpack('Q',d[i:i+8])[0];
		if ((val >= _TEXT_START)and(val <= _TEXT_END)or((val >= _PLT_START)and(val <= _PLT_END))):
			if ((val>=_PLT_START)and(val<=_PLT_END)):
				print "%x:%x,.plt"%(SECTION[sname]+i,val);
			elif (sname == ".rodata"):
				print "%x:%x,.data"%(SECTION[sname]+i,val);
			else:
				print "%x:%x,%s"%(SECTION[sname]+i,val,sname);
				
	return;

def read_rela_section_line(l):
	items = list();
	for item in l.split(" "):
		if (item != ""):
			items.append(item);
	if (len(items) < 5):
		items.append("");
	return items;

#for both rela.dyn and rela.plt section
# First read rela.dyn table to check the address inside.
# Second cross compare the import plt entry from rela.dyn to rela.plt and give the offset
# More important, for pie code, which linked with other library. 
# If a plt entry is static init, i.e., store in data section. 
# By scan the data section, you can not see since it is dynamic fill in by loader
# However, this variable/plt must be describle in .rela.dyn section, where the offset is belongs to data section
# So in this case, even there are nothing in data section, still give an entry
def read_rela_section(bname):
	global SECTION
	global _TEXT_START
	global _TEXT_END
	global _GOT_START
	global _GOT_END
	global _PLT_START
	global _PLT_END
	global _DATA_START
	global _DATA_END

	os.system("readelf -a ./%s --wide > %s.sym"%(bname,bname));
	with open("./%s.sym"%(bname),"rb") as f:
		d = f.readlines();
	
	#read rela.dyn table
	k = 0;
	while (k<len(d))and(d[k].find("Relocation section '.rela.dyn'")==-1):
		k += 1;
	if (k >= len(d)):
		return;
	d = d[k+2:]
	rela_dyn_dict = dict();
	for l in d:
		if (l == "\n"):
			break;
		items = read_rela_section_line(l);
		offset = int(items[0],16);
		val = int(items[3],16);
		name = items[4];
		if (name != ""):
			rela_dyn_dict[name] = offset;
		
		if ((val >= _TEXT_START)and(val <= _TEXT_END)or((val >= _PLT_START)and(val <= _PLT_END))):
			print "%x:%x,.got"%(offset,val);

	#read rela.plt table
	k = 0;
	while (k<len(d))and(d[k].find("Relocation section '.rela.plt'")==-1):
		k += 1;
	if (k >= len(d)):
		return;
	d = d[k+2:]
	rela_plt_dict = dict();
	for l in d:
		if (l == "\n"):
			break;
		items = read_rela_section_line(l);
		offset = int(items[0],16);
		val = int(items[3],16);
		name = items[4];
		if (val==0):
			val = search_plt_entries(name);
		if (name in rela_dyn_dict.keys()):
			#means the .rela.dyn have same entry for this plt entry, but the offset may differ, so add both
			if ((rela_dyn_dict[name]>=_DATA_START)and(rela_dyn_dict[name]<=_DATA_END)):
				print "%x:%x,.data"%(rela_dyn_dict[name],val);
			else:
				print "%x:%x,.rela.plt"%(rela_dyn_dict[name],val);

		print "%x:%x,.rela.plt"%(offset,val);

	return;

def read_sec_header_log(l,sec):
	global SECTION
	global _TEXT_START
	global _TEXT_END
	global _GOT_START
	global _GOT_END
	global _PLT_START
	global _PLT_END
	
	items = list();
	for item in l.split(sec)[1].split(" "):
		if item != "":
			items.append(item);
	print items;
	addr = int(items[1],16);
	size = int(items[3],16);
	return (addr,size);

def main():
	global SECTION
	global _TEXT_START
	global _TEXT_END
	global _GOT_START
	global _GOT_END
	global _PLT_START
	global _PLT_END
	global _DATA_START
	global _DATA_END

	bpath = os.path.abspath(sys.argv[1]);
	bname = bpath.split("/");
	bname = bname[len(bname)-1]+".strip";
	os.system("cp %s ./%s"%(bpath,bname));
	os.system("strip ./%s"%(bname));
	os.system("readelf -S ./%s --wide > section.log"%(bname));
	read_plt_entries();
	
	f = open("./section.log","r");
	d = f.readlines();
	f.close();
	for l in d:
		for sec in SECTION.keys():
			if (l.find(sec)!=-1):
				(addr,size) = read_sec_header_log(l,sec);
				SECTION[sec] = addr;
		if (l.find(" .text ")!=-1):
			(addr,size) = read_sec_header_log(l,' .text ');
			_TEXT_START = addr;
			_TEXT_END = addr+size;
		if (l.find(" .plt ")!=-1):
			(addr,size) = read_sec_header_log(l,' .plt ');
			_PLT_START = addr;
			_PLT_END = addr+size;
		if (l.find(" .got ")!=-1):
			(addr,size) = read_sec_header_log(l,' .got ');
			_GOT_START = addr;
			_GOT_END = addr+size;
		
		if (l.find(" .data ")!=-1):
			(addr,size) = read_sec_header_log(l,' .data ');
			_DATA_START = addr;
			_DATA_END = addr+size;

	for sec in SECTION.keys():
		read_section(sec,bname);
	read_rela_section(bname);



if __name__=="__main__":
	main();


