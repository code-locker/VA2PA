**Virtual address to physical address conversion code**

In this code I am creating a array using malloc()

Starting address of array i.e. Virtual address will be passed to virt_to_phys()

virt_to_phys() will return the physical address

We can read the data stored in physical address using devmem command

**Command to compile and run the code**

gcc v2p.c -o v2p
./v2p
