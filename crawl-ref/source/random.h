#ifndef RANDOM_H
#define RANDOM_H

#include <algorithm>  // iter_swap
#include <iterator>   // advance
#include <map>
#include <type_traits> // remove_reference
#include <vector>

#include "hash.h"

void seed_rng();
void seed_rng(uint32_t seed);
void seed_rng(uint64_t[], int);

uint32_t get_uint32(int generator = RNG_GAMEPLAY);
uint64_t get_uint64(int generator = RNG_GAMEPLAY);
bool coinflip();
int div_rand_round(int num, int den);
int rand_round(double x);
int div_round_up(int num, int den);
bool one_chance_in(int a_million);
bool x_chance_in_y(int x, int y);
bool x_chance_in_y(double x, double y);
int random2(int max);
int maybe_random2(int x, bool random_factor);
int maybe_random_div(int nom, int denom, bool random_factor);
int maybe_roll_dice(int num, int size, bool random);
int random_range(int low, int high);
int random_range(int low, int high, int nrolls);
double random_real();

// Resolve ambiguous calls to x_chance_in_y in favour of the double version
// if either parameter is floating-point, otherwise the integer version.
template <typename T, typename U>
bool x_chance_in_y(T x, U y)
{
    return (is_floating_point<T>::value || is_floating_point<U>::value)
         ? x_chance_in_y(double(x), double(y))
         : x_chance_in_y(int(x), int(y));
}

int random2avg(int max, int rolls);
int biased_random2(int max, int n);
int random2limit(int max, int limit);
int binomial(unsigned n_trials, unsigned trial_prob, unsigned scale = 100);
bool bernoulli(double n_trials, double trial_prob);
int fuzz_value(int val, int lowfuzz, int highfuzz, int naverage = 2);
int roll_dice(int num, int size);
bool decimal_chance(double percent);

int ui_random(int max);

/** Chooses one of the objects passed in at random (by value).
 *  @return One of the arguments.
 *
 *  @note All the arguments must be convertible to the type of the first.
 */
template <typename T, typename... Ts>
T random_choose(T first, Ts... rest)
{
    const T elts[] = { first, rest... };
    return elts[random2(1 + sizeof...(rest))];
}

/** Chooses one of the objects passed in at random (by reference).
 *
 * @return A reference to one of the arguments.
 *
 * @note All the arguments must be of a type compatible with the type of the
 *       first. Specifically, it must be possible to implicitly convert a
 *       pointer to each argument into the same type as a pointer to the first
 *       argument. So, for example, if the first argument is non-const, none
 *       of the subsequent subsequent arguments may be const.
 */
template <typename T, typename... Ts>
T& random_choose_ref(T& first, Ts&... rest)
{
    return *random_choose(&first, &rest...);
}

template <typename C>
auto random_iterator(C &container) -> decltype(container.begin())
{
    int pos = random2(container.size());
    auto it = container.begin();
    advance(it, pos);
    return it;
}

/**
 * Get a random weighted choice.
 *
 * Weights are assumed to be non-negative, but are allowed to be zero.
 * @tparam  V  A map, vector of pairs, etc., with the values of the
 *             map or the second elements of the pairs being integer
 *             weights.
 *
 * @param   choices  The collection of choice-weight pairs to choose from.
 *
 * @return  A pointer to the item in the chosen pair, or nullptr if all
 *          weights are zero. The pointer is const only if necessary.
 */
template <typename V>
auto random_choose_weighted(V &choices) -> decltype(&(begin(choices)->first))
{
    int total = 0;
    for (const auto &entry : choices)
        total += entry.second;
    int r = random2(total);
    int sum = 0;
    for (auto &entry : choices)
    {
        sum += entry.second;
        if (sum > r)
            return &entry.first;
    }
    return nullptr;
}

/**
 * Get an index for a random weighted choice using a fixed vector of
 * weights.
 *
 * Entries with a weight <= 0 are skipped.
 * @param choices The fixed vector with weights for each item.
 *
 * @return  A index corresponding to the selected item, or -1 if all
 *          weights were skipped.
 */
template <typename T, int SIZE>
int random_choose_weighted(const FixedVector<T, SIZE>& choices)
{
    int total = 0;
    for (auto weight : choices)
        if (weight > 0)
            total += weight;

    int r = random2(total);
    int sum = 0;
    for (int i = 0; i < SIZE; ++i)
    {
        if (choices[i] <= 0)
            continue;

        sum += choices[i];
        if (sum > r)
            return i;
    }
    return -1;
}

template <typename T>
T random_choose_weighted(int cweight, T curr)
{
    return curr;
}

template <typename T, typename... Args>
T random_choose_weighted(int cweight, T curr, int nweight, T next, Args... args)
{
    return random_choose_weighted<T>(cweight + nweight,
                                     random2(cweight+nweight) < nweight ? next
                                                                        : curr,
                                     args...);
}

/**
 * Get a random weighted choice from a iterator, using a weight function.
 *
 * Weights are assumed to be non-negative, but are allowed to be zero.
 *
 * @tparam Iterator An iterator type such that incrementing an iterator
 *         does not affect copies of that iterator (that is, almost a
 *         ForwardIterator, but possibly without a default constructor);
 *         and also convertible to bool such that past-the-end iterators
 *         are false and others are true.
 *
 * @tparam WeightFn A function or function-like object type taking as its
 *         parameter an object returned by Iterator::operator*, and returning
 *         a type convertible to integer.
 *
 * @param xs     The beginning of the range.
 * @param weight The weight function.
 *
 * @return  An iterator to the chosen item, or a past-the-end iterator if
 *          all weights are zero.
 *
 * @note Intended for use with iterators such as distance_iterator.
 */
template<typename Iterator, typename WeightFn>
Iterator random_choose_weighted(Iterator xs, WeightFn weight)
{
    // Iterator might not have a default constructor, but it does have
    // a copy constructor, so initialise to that.
    Iterator result { xs };
    bool found = false;

    // Use remove_reference just in case weight() returns a reference
    // into the pointed-to data structure.
    typename remove_reference<decltype(weight(*xs))>::type totalweight = 0;
    while (xs)
    {
        auto cweight = weight(*xs);
        totalweight += cweight;
        if (x_chance_in_y(cweight, totalweight))
        {
            result = xs;
            found = true;
        }
        ++xs;
    }
    return found ? result : xs; // xs is past-the-end
}

/**
 * Get a random weighted choice from a range, using a weight function.
 *
 * Weights are assumed to be non-negative, but are allowed to be zero.
 *
 * @tparam Iterator An iterator type such that incrementing an iterator
 *         does not affect copies of that iterator (that is, almost a
 *         ForwardIterator, but possibly without a default constructor).
 *
 * @tparam WeightFn A function or function-like object type taking as its
 *         parameter an object returned by Iterator::operator*, and returning
 *         a type convertible to integer.
 *
 * @param start  The beginning of the range.
 * @param end    The past-the-end iterator of the range.
 * @param weight The weight function.
 *
 * @return  An iterator to the chosen item, or end if all weights are zero.
 */
template<typename Iterator, typename WeightFn>
Iterator random_choose_weighted(Iterator start, Iterator end, WeightFn weight)
{
    Iterator result { end };
    typename remove_reference<decltype(weight(*start))>::type totalweight = 0;
    while (start != end)
    {
        auto cweight = weight(*start);
        totalweight += cweight;
        if (x_chance_in_y(cweight, totalweight))
            result = start;
        ++start;
    }
    return result;
}

struct dice_def
{
    int num;
    int size;

    constexpr dice_def() : num(0), size(0) {}
    constexpr dice_def(int n, int s) : num(n), size(s) {}
    int roll() const;
};

constexpr dice_def CONVENIENT_NONZERO_DAMAGE{42, 1};

dice_def calc_dice(int num_dice, int max_damage);

// I must be a random-access iterator.
template <typename I>
void shuffle_array(I begin, I end)
{
    size_t n = end - begin;
    while (n > 1)
    {
        const int i = random2(n);
        n--;
        iter_swap(begin + i, begin + n);
    }
}

template <typename T>
void shuffle_array(T &vec)
{
    shuffle_array(begin(vec), end(vec));
}

template <typename T>
void shuffle_array(T *arr, int n)
{
    shuffle_array(arr, arr + n);
}

/**
 * A defer_rand object represents an infinite tree of random values, allowing
 * for a much more functional approach to randomness. defer_rand values which
 * have been used should not be copy-constructed. Querying the same path
 * multiple times will always give the same result.
 *
 * An important property of defer_rand is that, except for rounding,
 * `float(r.random2(X)) / X == float(r.random2(Y)) / Y` for all `X` and `Y`.
 * In other words:
 *
 * - The parameter you use on any given call does not matter.
 * - The object stores the fraction, not a specific integer.
 * - random2() is monotonic in its argument.
 *
 * Rephrased: The first time any node in the tree has a method called on
 * it, a random float between 0 and 1 (the fraction) is generated and stored,
 * and this float is combined with the method's parameters to arrive at
 * the result. Calling the same method on the same node with the same
 * parameters will always give the same result, while different parameters
 * or methods will give different results (though they'll all use the same
 * float which was generated by the first method call). Each node in the
 * tree has it's own float, so the same method+parameters on different
 * nodes will get different results.
 */
class defer_rand
{
    vector<uint32_t> bits;
    map<int, defer_rand> children;

    bool x_chance_in_y_contd(int x, int y, int index);
public:
    // TODO It would probably be a good idea to have some sort of random
    // number generator API, and the ability to pass RNGs into any function
    // that wants them.
    bool x_chance_in_y(int x, int y) { return x_chance_in_y_contd(x,y,0); }
    bool one_chance_in(int a_million) { return x_chance_in_y(1,a_million); }
    int random2(int maxp1);

    int random_range(int low, int high);
    int random2avg(int max, int rolls);

    defer_rand& operator[] (int i);
};

template<typename Iterator>
int choose_random_weighted(Iterator beg, const Iterator end)
{
    int totalweight = 0;
    int result = -1;
    for (int count = 0; beg != end; ++count, ++beg)
    {
        totalweight += *beg;
        if (random2(totalweight) < *beg)
            result = count;
    }
    ASSERT(result >= 0);
    return result;
}

#endif
