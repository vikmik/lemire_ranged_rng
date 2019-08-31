#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

static uint32_t get_random_uniform_32bits_integer() {
    /* Awful way to generate a 32bit random integer using the standard library, assuming RAND_MAX == 2^31 - 1
     * In theory, rand() does not have any guarantee of uniformity, and RAND_MAX may not even be "a power of 2 minus one" on the paper.
     * So for this little exercise, let's not bother with handling obscure values of RAND_MAX/the non-uniformity, and Let's Just Assume It Works ™.
     */
    uint32_t result = (uint32_t)rand(); // This only gives us 31bits on Linux
    result |= (((uint32_t)rand()) & 1) << 31;
    return result;
}

/* Decomposes n*range modulo 2^32, such as n*range == i*2^32 + j, with 0 <= j < 2^32
 * Stores the result in i and j, telling the caller in which interval - A(i) or R(i), defined below - n*range is in.
 */
static void compute_interval(uint32_t n, uint32_t range, uint32_t * i, uint32_t * j) {
    // We need 64 bits to store the result of the n*range multiplication.
    const uint64_t n_times_range = n * (uint64_t)range;

    // To compute (n * range) / 2^32, take the 32 most significant bits of n*range
    *i = (uint32_t)(n_times_range >> 32);

    // To compute (n * range) % 2^32, take the 32 least significant bits of n*range
    *j = (uint32_t)n_times_range;
}

/* Computes uniformly distributed random numbers in [0, range[ using Lemire's algorithm, and stores the result in 'result'.
 * Returns 0 on success, and returns a non-zero value if range == 0.
 */
int lemire_rng(uint32_t range, uint32_t * result) {
    if (range == 0) {
        return 1;
    }

    uint32_t i, j;
    uint32_t n = get_random_uniform_32bits_integer();

    /* Lemire's algorithm takes random numbers from [0, 2^32[ and maps them to [0, range[, using 2 consecutive transformations:
     * f: n -> n*range   -- maps [0, 2^32[ to the set of multiples of 'range' in [0, range*2^32[
     * g: m -> m / 2^32   -- cheap integer division via bit masking; maps [0, range*2^32[ to [0, range[
     * However, even if 'n' is uniformly distributed over [0, 2^32[, g(f(n)) is generally not uniformly distributed over [0, range[.
     * We have to do a little more work to preserve the uniformity.
     *
     * Here's how it works:
     * [0, range*2^32[ is divided into 'range' intervals of the form I(i) = [i * 2^32, (i+1) * 2^32[ (with 0 <= i < range)
     * Each I(i) interval is then subdivided in 2:
     *   R(i) = [i * 2^32, i * 2^32 + 2^32 % range[  -- possibly empty if 'range' is a power of 2
     *   A(i) = [i * 2^32 + 2^32 % range, (i+1) * 2^32[
     * Now, let's define the set U = { n in [0, 2^32[ such as f(n) is in not in any of the R(i) intervals }
     * The main result from Lemire is that if 'n' is uniformly distributed in U, then g(f(n)) is uniformly distributed in [0, range[.
     *
     * Building a uniform distribution on U is done by using a uniform distribution on [0, 2^32[, discarding integers n for which f(n) fall
     * into any of the R(i) intervals (rejection sampling, using the definition of U).
     * Note for the reader: for A(i) and R(i), 'A' is for 'Accept', and 'R' is for 'Reject'.
     * 
     * Given a random 32 bits number 'n', we decompose f(n) modulo 2^32, to do the rejection sampling:
     * f(n) = n*range = i*2^32 + j
     * where:
     *   i = n * range / 2^32 == g(f(n))
     *   j = n * range % 2^32
     */

    compute_interval(n, range, &i, &j);
    /* We have n*range == i * 2^32 + j
     *
     * This decomposition allows us to determine if f(n) falls in A(i) or in R(i), necessary to implement the rejection sampling.
     * f(n) being in R(i) is equivalent to j being in [0, 2^32 % range[
     */

    /* The following if block is not required, but removes the need for a division in a lot of cases.
     * This is especially true when 'range' is relatively small compared to 2^32.
     */
    if (j >= range) {
        // j >= range implies that j > (2^32 % range), so f(n) is not in the R(i) interval. No rejection needed!
        *result = i;
        return 0;
    }

    /* We need to compare 'j' against '2^32 % range', but 2^32 doesn't fit in a uint32_t.
     * In 32 bit arithmetics, 2^32 % range == (2^32 - range) % range == (-range) % range
     */
    uint32_t rejection_interval_upper_bound = (-range) % range;
    while (j < rejection_interval_upper_bound) {
        // Reject this number; pick a new one and try again
        n = get_random_uniform_32bits_integer();
        compute_interval(n, range, &i, &j);
    }

    *result = i;
    return 0;
}

int main(void) {
    const uint32_t range = 12345;
    uint32_t result = 0;

    srand(time(NULL));

    // Generate a few of random numbers in [0, range[.
    for (int i=0; i<10; ++i) {
        lemire_rng(range, &result);
        printf("Random number in [0, %u[: %u\n", range, result);
    }
    return 0;
}
