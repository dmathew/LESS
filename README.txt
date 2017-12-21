README

We have tested all the code only on virtual machines, using VMWare. Our implementation

1. Compile the modified kernel and boot into it
2. Compile and insert the LESS module (code in less-module/)
3. Compile application code using make (code in less/tcp/)
4. Create a folder named acct
5. Create a folder named /less on a node and share it writable using NFS
6. Mount the folder created in step #5 on /less at the other node
7. Create a new group named less with gid 7001
8. Start LESS using ./less <ip_of_other_node>
9. Do the same at the other node also

To create migrateable load, run a test program of group less and setgid bit on. This can be done using the following commands:
chgrp less <exe_file>
chmod 2755 <exe_file>
(Or else, use the script lessen: ./lessen <exe_file>)

We have tested the communication with TCP only. However, we have included the UDP code too (less/less_udp.tar.gz).

Good luck!

Contact info for detailed report:
	danieljmathew@gmail.com
	Shaneed C M