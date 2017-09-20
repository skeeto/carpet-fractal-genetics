#include <stdio.h>

#define SCALE 9

int main(void)
{
    int ncolors, niters;
    static unsigned char rules[256][9];

    scanf("%d%d", &ncolors, &niters);
    for (int i = 0; i < ncolors; i++)
        for (int c = 0; c < 9; c++)
            scanf("%hhd", rules[i] + c);

    int size = 1;
    for (int i = 0; i < niters; i++)
        size *= 3;

    printf("P2\n%d %d\n%d\n", size * SCALE, size * SCALE, ncolors - 1);
    for (int y = 0; y < size * SCALE; y++) {
        for (int x = 0; x < size * SCALE; x++) {
            int c = 0;
            int px = x / SCALE;
            int py = y / SCALE;
            int div = size / 3;
            for (int i = 1; div > 0; i++) {
                c = rules[c][py / div * 3 + px / div];
                px %= div;
                py %= div;
                div /= 3;
            }
            printf("%d\n", c);
        }
    }
}
