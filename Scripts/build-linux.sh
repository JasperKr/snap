if [ -z "$1" ]; then
    CONFIG="Release"
else
    CONFIG="$1"
fi

rm -f ./build/snap

FLAGS=""

if [ "$CONFIG" = "Debug" ]; then
  FLAGS="$FLAGS -g -O1 -ftime-trace -fno-omit-frame-pointer"
  CONFIG="Debug"
elif [ "$CONFIG" = "Profile" ]; then
  FLAGS="$FLAGS -DTRACY_ENABLE=1 -DTRACY_WAIT_FOR_CLIENT=1 -O3"
  CONFIG="RelWithDebInfo"
elif [ "$CONFIG" == "Release" ]; then
  FLAGS="$FLAGS -O3 -ffast-math -flto=thin -funsafe-math-optimizations -march=native"
elif [ "$CONFIG" == "RelWithDebInfo" ]; then
  FLAGS="$FLAGS -O2 -g -ftime-trace"
else
  echo "Unknown configuration: $CONFIG"
  exit 1
fi

echo "Building with configuration: $CONFIG"
echo "Using flags: $FLAGS"

cmake -G Ninja -B build \
  -DCMAKE_BUILD_TYPE=$CONFIG \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_FLAGS="-Wc23-extensions $FLAGS" \
  -DCMAKE_C_FLAGS="-Wc23-extensions $FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="-rdynamic" \
  -DENABLE_RTTI=ON \
  -DENABLE_EXCEPTIONS=ON

cmake --build build

./build/lua_stub_gen

# Make sure to append amdgpu.ppfeaturemask=0xffffffff to GRUB_CMDLINE_LINUX_DEFAULT in /etc/default/grub (space-separated).
# Otherwise profiling may not work correctly.
# After modifying /etc/default/grub, run `sudo update-grub` or `sudo grub-mkconfig -o /boot/grub/grub.cfg` and reboot.
# You can verify the setting by running `cat /proc/cmdline` and checking for amdgpu.ppfeaturemask=0xffffffff

if [ "$2" == "profile" ]; then
  #rmv radeon memory visualizer. Needs /opt/radeon-gpu-profiler/scripts/setup.sh to be run beforehand
  #rgp for radeon gpu profiler.
  #rra for radeon raytracing analyzer.
  AMD_VULKAN_ICD=AMDVLK SDL_VIDEODRIVER=x11 MESA_VK_TRACE=rgp MESA_VK_TRACE_TRIGGER=/tmp/trigger ./build/snap src/Scripting/main.lua
fi

# if second argument is "run", run the built executable
if [ "$2" == "run" ]; then
  ./build/snap src/Scripting/main.lua
fi