#include <emu_cxx_utils/replicated.h>
#include <emu_cxx_utils/striped_array.h>
#include <emu_cxx_utils/transform.h>
#include <emu_cxx_utils/fill.h>

#include <common.h>

using namespace emu::execution;

struct stream {
    emu::striped_array<long> _a, _b, _c;
    explicit stream(long n) : _a(n), _b(n), _c(n) {}

    stream(const stream& other, emu::shallow_copy shallow)
    : _a(other._a, shallow)
    , _b(other._b, shallow)
    , _c(other._c, shallow)
    {
    }

    void init()
    {
        emu::parallel::fill(_a.begin(), _a.end(), 1L);
        emu::parallel::fill(_b.begin(), _b.end(), 2L);
        emu::parallel::fill(_c.begin(), _c.end(), -1L);
    }

    void run()
    {
        emu::parallel::transform(par_limit, _a.begin(), _a.end(), _b.begin(), _c.begin(),
            [](long a, long b) { return a + b; }
        );
    }

    void validate()
    {
        for (long i = 0; i < _c.size(); ++i) {
            if (_c[i] != 3) {
                LOG("VALIDATION ERROR: c[%li] == %li (supposed to be 3)\n", i, _c[i]);
                exit(1);
            }
        }
    }
};

struct arguments {
    long _log2_num_elements;
    long _num_trials;

    static arguments
    parse(int argc, char** argv)
    {
        arguments args;
        if (argc != 3) {
            LOG("Usage: %s log2_num_elements num_trials\n", argv[0]);
            exit(1);
        } else {
            args._log2_num_elements = atol(argv[1]);
            args._num_trials = atol(argv[1]);

            if (args._log2_num_elements <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
            if (args._num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
        }
        return args;
    }
};


int main(int argc, char * argv[])
{
    auto args = arguments::parse(argc, argv);

    long n = 1L << args._log2_num_elements;
    auto bench = emu::make_repl_copy<stream>(n);
#ifndef NO_VALIDATE
    bench->init();
#endif
    for (long trial = 0; trial < args._num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin("stream");
        bench->run();
        double time_ms = hooks_region_end();
        double bytes_per_second = time_ms == 0 ? 0 :
            (n * sizeof(long) * 3) / (time_ms / 1000);
        LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
    }
#ifndef NO_VALIDATE
    bench->validate();
#endif
}