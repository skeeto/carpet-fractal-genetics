#include <time.h>
#include <float.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TARGET      81
#define POPULATION  200
#define KEEPSIZE    4
#define BREEDSIZE   10
#define REGENSIZE   5
#define SCALE       10

#define DX(i) ((int)((0x0489a621UL >> (4 * (i) + 0)) & 3) - 1)
#define DY(i) ((int)((0x0489a621UL >> (4 * (i) + 2)) & 3) - 1)

#define COUNTOF(a) (int)(sizeof(a) / sizeof(*a))

/* Simplified PCG PRNG */
static uint32_t
pcg32(uint64_t *s)
{
    uint64_t m = 0x9b60933458e17d7d;
    uint64_t a = 0xd737232eeccdf7ed;
    *s = *s * m + a;
    int shift = 29 - (*s >> 61);
    return *s >> shift;
}

/* Read an ASCII PGM image returning the number of colors */
static int
pgm_load(unsigned char *buf, FILE *in)
{
    int w, h, d;
    if (fscanf(in, "P2 %d %d %d", &w, &h, &d) != 3)
        return 0;
    if (w != TARGET || h != TARGET || d < 1 || d > UCHAR_MAX)
        return 0;
    for (int i = 0; i < TARGET * TARGET; i++) {
        int v;
        fscanf(in, "%d", &v);
        if (v < 0 || v > d)
            return 0;
        buf[i] = v;
    }
    return d + 1;
}

/* Write an ASCII PGM image at the given scale */
static void
pgm_write(unsigned char *buf, int ncolors, int scale, FILE *o)
{
    fprintf(o, "P2\n%d %d\n%d\n", TARGET * scale, TARGET * scale, ncolors - 1);
    for (int y = 0; y < TARGET * scale; y++)
        for (int x = 0; x < TARGET * scale; x++)
            fprintf(o, "%d\n", buf[(y / scale) * TARGET + (x / scale)]);
}

/* Write a binary PPM at the given scale */
static void
ppm_write(unsigned char *buf, int ncolors, int scale, FILE *o)
{
    fprintf(o, "P6\n%d %d\n255\n", TARGET * scale, TARGET * scale);
    for (int y = 0; y < TARGET * scale; y++) {
        for (int x = 0; x < TARGET * scale; x++) {
            int v = buf[(y / scale) * TARGET + (x / scale)];
            v = v * 255 / (ncolors - 1);
            fputc(v, o);
            fputc(v, o);
            fputc(v, o);
        }
    }
}

/* Return the error between two images */
static double
score(unsigned char *a, unsigned char *b)
{
    double error = 0.0;
    for (int y = 0; y < TARGET; y++) {
        for (int x = 0; x < TARGET; x++) {
            int c = 1;
            int amean = a[y * TARGET + x];
            int bmean = b[y * TARGET + x];
            for (int i = 0; i < 8; i++) {
                int xx = x + DX(i);
                int yy = y + DY(i);
                if (xx >= 0 && xx < TARGET && yy >= 0 && yy < TARGET) {
                    amean += a[yy * TARGET + xx];
                    bmean += b[yy * TARGET + xx];
                    c++;
                }
            }
            double delta = amean / (double)c - bmean / (double)c;
            error += delta * delta;
        }
    }
    return error;
}

/* Render an image from a given ruleset */
static void
render(unsigned char *buf, unsigned char rules[][9])
{
    for (int y = 0; y < TARGET; y++) {
        for (int x = 0; x < TARGET; x++) {
            int c = 0;
            int px = x;
            int py = y;
            int div = TARGET / 3;
            for (int i = 1; div > 0; i++) {
                c = rules[c][py / div * 3 + px / div];
                px %= div;
                py %= div;
                div /= 3;
            }
            buf[y * TARGET + x] = c;
        }
    }
}

/* Generate a random ruleset */
static void
random_rules(unsigned char rules[][9], int ncolors, uint64_t *rng)
{
    for (int i = 0; i < ncolors; i++)
        for (int c = 0; c < 9; c++)
            rules[i][c] = pcg32(rng) % ncolors;
}

/* Randomly mix two rulesets into a new ruleset */
static void
breed(unsigned char o[][9],
      unsigned char a[][9],
      unsigned char b[][9],
      int ncolors,
      uint64_t *rng)
{
    uint32_t select = 0;
    for (int i = 0; i < ncolors; i++) {
        if (i % 32 == 0)
            select = pcg32(rng);
        unsigned char *src = (select >> (i % 32)) & 1 ? a[i] : b[i];
        memcpy(o[i], src, sizeof(o[i]));
    }

    int mutations = pcg32(rng) % ncolors;
    for (int i = 0; i < mutations; i++) {
        select = pcg32(rng);
        int r = (select >>  0) % ncolors;
        int c = (select >>  8) % 9;
        int t = (select >> 12) % ncolors;
        o[r][c] = t;
    }
}

static int
dblcmp(const void *pa, const void *pb)
{
    double a = *(double *)pa;
    double b = *(double *)pb;
    if (a < b)
        return -1;
    else if (a > b)
        return 1;
    return 0;
}

/* Write a ruleset out */
static void
rule_write(unsigned char rules[][9], int ncolors, FILE *o)
{
    int niter = 0;
    for (long t = TARGET; t > 0; t /= 3)
        niter++;
    fprintf(o, "%d %d\n", ncolors, niter);
    for (int i = 0; i < ncolors; i++)
        for (int c = 0; c < 9; c++)
            fprintf(o, "%d%c", rules[i][c], " \n"[c == 8]);
}

int main(void)
{
    uint64_t rng[] = {0x9e8480dd162324e1};
    unsigned char rules[POPULATION][256][9];
    unsigned char input[TARGET * TARGET];

    int ncolors = pgm_load(input, stdin);
    if (!ncolors) {
        fputs("Invalid input\n", stderr);
        return 1;
    }

    *rng ^= time(0);
    for (size_t i = 0; i < COUNTOF(rules); i++)
        random_rules(rules[i], ncolors, rng);

    FILE *video = fopen("video.ppm", "wb");
    double global_best = DBL_MAX;
    long long bestg = LLONG_MAX / 4;
    for (long long g = 0; g < 2 * bestg + 1000; g++) {
        struct {
            double score;
            size_t i;
        } scores[COUNTOF(rules)];
        unsigned char best[BREEDSIZE][256][9];

        /* Find the 10 best rulesets */
        #pragma omp parallel for
        for (size_t i = 0; i < COUNTOF(rules); i++) {
            unsigned char buf[TARGET * TARGET];
            render(buf, rules[i]);
            scores[i].score = score(input, buf);
            scores[i].i = i;
        }
        qsort(scores, COUNTOF(scores), sizeof(*scores), dblcmp);

        /* Breed next generation */
        for (int i = 0; i < COUNTOF(best); i++)
            memcpy(best[i], rules[scores[i].i], sizeof(best[i]));
        for (int i = 0; i < KEEPSIZE; i++)
            memcpy(rules[i], best[i], sizeof(best[i]));
        for (int i = KEEPSIZE; i < COUNTOF(rules) - REGENSIZE; i++) {
            uint32_t select = pcg32(rng);
            int a = (select >>  0) % 10;
            int b = (select >> 16) % 10;
            breed(rules[i], best[a], best[b], ncolors, rng);
        }
        for (int i = COUNTOF(rules) - REGENSIZE; i < COUNTOF(rules); i++)
            random_rules(rules[i], ncolors, rng);

        /* Report on progress */
        if (scores[0].score < global_best) {
            bestg = g;
            global_best = scores[0].score;

            /* Write out the current best image */
            unsigned char buf[TARGET * TARGET];
            FILE *image = fopen("_progress.pgm", "w");
            render(buf, best[0]);
            pgm_write(buf, ncolors, SCALE, image);
            fclose(image);
            rename("_progress.pgm", "progress.pgm");

            /* Write out best image as video frame */
            ppm_write(buf, ncolors, SCALE, video);
            fflush(video);

            /* Write out the ruleset */
            FILE *save = fopen("best.txt", "w");
            rule_write(best[0], ncolors, save);
            fclose(save);
        }
        printf("%12lld %f %f\n", g, global_best, scores[0].score);
    }
}
