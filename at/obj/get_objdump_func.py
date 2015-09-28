import os,sys

bname = os.path.abspath(sys.argv[1]);

os.system("objdump -d %s > ./tmp.s"%(bname));
os.system('cat ./tmp.s | grep ">:" > objdump.log');
os.system('cat ./tmp.s | grep "@plt>:" > plt.log');

