# SYCL Host Compilation

The SYCL Host Compilation flow aims at treating the host CPU as a "first class citizen", providing a SYCL implementation that targets CPUs of various different architectures, with no other dependencies than DPC++ itself, while bringing performances comparable to state-of-the-art CPU backends.

# Compiler and runtime options

The SYCL Host Compilation flow is enabled by the `-fsycl-host-compilation` compiler option, e.g.

```
clang++ -fsycl -fsycl-host-compilation <input> -o <output>
```

This will perform automatically all the compilation stages, it is also possible to manually perform all the necessary compiler invocations. This is more verbose but allows the user to use an arbitrary host compiler for the second compilation stage:

```
#device compiler
clang++ -fsycl-device-only -fsycl-host-compilation -Xclang -fsycl-int-header=<integration-header> \
  -Xclang -fsycl-hc-header=<host-compilation-int-header> \
  -Xclang -fsycl-int-footer=<integration-footer> <input> -o <device-ir>
#host compiler
clang++ -fsycl-is-host -include <integration-header> \
  -include <host-compilation-int-header> \
  -include <integration-footer> \
  <intput> -c -o <host-o>
#compile device IR
clang++ <device-ir> -o <device-o>
#link
clang++ -L<sycl-lib-path> -lsycl <device-o> <host-o> -o <output>
```

Our implementation currenyl piggy backs on the original (library only) SYCL Host Device, therefore in order to run an application compiled with `-fsycl-host-compilation`, you need to set the environment variable `ONEAPI_DEVICE_SELECTOR=host:*` to make sure that the SYCL runtime chooses the host device to execute the application.

# Supported features and limitations

The SYCL Host Compilation flow is still WIP, and several core SYCL features are currently unsupported. Currently only `parallel_for`s over `sycl::range` are supported, attempting to use `local_size`, `local `barrier` and any math builtin will most likely fail with an `undefined reference` error at link time. Examples of supported applications can be found in the [runtime tests](./test/host_compilation).


