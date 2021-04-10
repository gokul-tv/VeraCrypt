namespace VeraCrypt
{
    template <typename T>
    static void Hexdump(T& buf, int len) {
        for (int i = 0; i < len; i++) {
            printf("%02x", (uint8_t)buf[i]);
            if (i % 16 == 15) {
                printf("\n");
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }
}
