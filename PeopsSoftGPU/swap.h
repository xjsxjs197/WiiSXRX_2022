
// byteswappings

#define SWAP16(x) (((x)>>8 & 0xff) | ((x)<<8 & 0xff00))
#define SWAP32(x) (((x)>>24 & 0xfful) | ((x)>>8 & 0xff00ul) | ((x)<<8 & 0xff0000ul) | ((x)<<24 & 0xff000000ul))

// big endian config
#define HOST2LE32(x) SWAP32(x)
#define HOST2BE32(x) (x)
#define LE2HOST32(x) SWAP32(x)
#define BE2HOST32(x) (x)

#define HOST2LE16(x) SWAP16(x)
#define HOST2BE16(x) (x)
#define LE2HOST16(x) SWAP16(x)
#define BE2HOST16(x) (x)

#define GETLEs16(X) ((short)GETLE16((unsigned short *)X))
#define GETLEs32(X) ((short)GETLE32((unsigned short *)X))

#ifdef _BIG_ENDIAN
// upd xjxjs197 start
inline unsigned short GETLE16(unsigned short *ptr) {
    unsigned short ret; __asm__ ("lhbrx %0, 0, %1" : "=r" (ret) : "r" (ptr));
    return ret;
}
inline unsigned long GETLE32(unsigned long *ptr) {
    unsigned long ret;
    __asm__ ("lwbrx %0, 0, %1" : "=r" (ret) : "r" (ptr));
    return ret;
}
inline unsigned long GETLE16D(unsigned long *ptr) {
    unsigned long ret;
    __asm__ ("lwbrx %0, 0, %1\n"
             "rlwinm %0, %0, 16, 0, 31" : "=r" (ret) : "r" (ptr));
    return ret;
}

inline void PUTLE16(unsigned short *ptr, unsigned short val) {
    __asm__ ("sthbrx %0, 0, %1" : : "r" (val), "r" (ptr) : "memory");
}
inline void PUTLE32(unsigned long *ptr, unsigned long val) {
    __asm__ ("stwbrx %0, 0, %1" : : "r" (val), "r" (ptr) : "memory");
}
// upd xjxjs197 end
#else // _BIG_ENDIAN
#define GETLE16(X) ((unsigned short *)X)
#define GETLE32(X) ((unsigned long *)X)
#define GETLE16D(X) ({unsigned long val = GETLE32(X); (val<<16 | val >> 16)})
#define PUTLE16(X, Y) {((unsigned short *)X)=(unsigned short)X}
#define PUTLE32(X, Y) {((unsigned long *)X)=(unsigned long)X}
#endif //!_BIG_ENDIAN
