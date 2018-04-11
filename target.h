/*
These are internal formats for generated code...

IP and ports use the same structure, but for ports it's about the probability distribution.
*/

struct range_cfg
{
 int num;
 unsigned int max_idx;  // number of addresses - 1, can't store 0
                        // for ports, number of distribution points - 1
 unsigned int mask; // for gen_rand32
 
 unsigned int ranges[]; // pairs of end_addr, sofar - 1
                        // for ports, pairs of port, sofar - 1
};

int random_ip(struct range_cfg *ranges);
int random_port(struct range_cfg *ports);

struct range_cfg *load_range_cfg(const char *file);
struct range_cfg *make_port_ranges(const char *portspec);
