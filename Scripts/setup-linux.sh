#!/usr/bin/env bash
set -e


read -p "This script will fetch all dependencies for the project. Do you want to proceed? (y/n/o)
Yes / No / Override: " -n 1 -r
echo    # move to a new line
if [[ $REPLY =~ ^[Oo]$ ]]; then
    override=1
elif [[ $REPLY =~ ^[Yy]$ ]]; then
    override=0
else
    echo "Aborting."
    exit 1
fi

git submodule update --init --recursive

# Create include folders
mkdir -p include/tl include/vma include/stb include/volk


# Fetch TL expected.hpp
if [ "$override" -eq 1 ] || [ ! -f "include/tl/expected.hpp" ]; then
    echo "Fetching tl::expected..."
    curl -L https://raw.githubusercontent.com/TartanLlama/expected/master/include/tl/expected.hpp -o include/tl/expected.hpp
else
    echo "tl::expected already exists, skipping fetch."
fi


# Fetch VMA
if [ "$override" -eq 1 ] || [ ! -f "include/vma/vk_mem_alloc.h" ]; then
    echo "Fetching Vulkan Memory Allocator..."
    mkdir -p include/vma
    curl -L https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h -o include/vma/vk_mem_alloc.h
else
    echo "Vulkan Memory Allocator already exists, skipping fetch."
fi


# Fetch Volk
if [ "$override" -eq 1 ] || [ ! -f "include/volk/volk.h" ] || [ ! -f "include/volk/volk.c" ]; then
    echo "Fetching Volk..."
    mkdir -p include/volk
    curl -L https://raw.githubusercontent.com/zeux/volk/master/volk.h -o include/volk/volk.h
    curl -L https://raw.githubusercontent.com/zeux/volk/master/volk.c -o include/volk/volk.c
else
    echo "Volk already exists, skipping fetch."
fi


# Fetch stb_image
if [ "$override" -eq 1 ] || [ ! -f "include/stb/stb_image.h" ]; then
    echo "Fetching stb_image..."
    mkdir -p include/stb
    curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o include/stb/stb_image.h
else
    echo "stb_image already exists, skipping fetch."
fi


# Fetch stb_perlin
if [ "$override" -eq 1 ] || [ ! -f "include/stb/stb_perlin.h" ]; then
    echo "Fetching stb_perlin..."
    mkdir -p include/stb
    curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_perlin.h -o include/stb/stb_perlin.h
else
    echo "stb_perlin already exists, skipping fetch."
fi


# Fetch embree pre-built binaries for Linux x86_64
URL="https://github.com/embree/embree/releases/download/v4.4.0/embree-4.4.0.x86_64.linux.tar.gz"
if [ "$override" -eq 1 ] || [ ! -d "include/embree" ] || [ -z "$(ls -A include/embree)" ]; then
    mkdir -p temp_embree
    curl -L "$URL" -o "temp_embree/embree.tar.gz"
    tar -xzf "temp_embree/embree.tar.gz" -C temp_embree
    mkdir -p include/embree
    cp -r temp_embree/include/embree4/* include/embree/
    mkdir -p lib/embree
    cp -r temp_embree/lib/* lib/embree/
    rm -rf temp_embree
fi


# If "./include/slang/" is missing or empty, or override is set:
if [ "$override" -eq 1 ] || [ ! -d "./include/slang/" ] || [ -z "$(ls -A ./include/slang/)" ]; then
    URL="https://github.com/shader-slang/slang/releases/download/v2026.19/slang-2026.19-linux-x86_64.zip"
    TEMP_DIR="./slang_download"

    if [ ! -d "$TEMP_DIR" ]; then
        mkdir -p "$TEMP_DIR"
    fi

    echo "Fetching slang..."
    curl -L "$URL" -o "$TEMP_DIR/slang.zip"
    echo "Extracting slang..."
    mkdir -p "$TEMP_DIR/contents"
    unzip -q "$TEMP_DIR/slang.zip" -d "$TEMP_DIR/contents"
    if [ ! -d "./include/slang/" ]; then
        mkdir -p "./include/slang/"
    fi
    cp -r "$TEMP_DIR/contents/include/"* "./include/slang/"
    if [ ! -d "./bin/slang/" ]; then
        mkdir -p "./bin/slang/"
    fi
    find "$TEMP_DIR/contents/bin" -maxdepth 1 -type f -exec cp {} ./bin/slang/ \;
    if [ ! -d "./lib/slang/" ]; then
        mkdir -p "./lib/slang/"
    fi
    find "$TEMP_DIR/contents/lib" -maxdepth 1 -type f -exec cp {} ./lib/slang/ \;
    echo "Cleaning up..."
    rm -rf "$TEMP_DIR"
fi


if [ "$override" -eq 1 ] || [ ! -f "include/float16_t/float16_t.hpp" ]; then
    echo "Fetching float-16"
    URL="https://github.com/fengwang/float16_t.git"
    if [ ! -d "float16_t_temp" ]; then
        git clone --depth 1 "$URL" float16_t_temp
    fi
    mkdir -p include/float16_t
    cp float16_t_temp/float16_t.hpp include/float16_t/float16_t.hpp
    rm -rf float16_t_temp
fi


# Download https://github.com/spnda/fastgltf.git to temp/fastgltf and copy temp/fastgltf/include/fastgltf/* to include/fastgltf/
URL="https://github.com/spnda/fastgltf.git"
if [ "$override" -eq 1 ] || [ ! -d "include/fastgltf" ]; then
    rm -rf include/fastgltf
    git clone --depth 1 "$URL" include/fastgltf
fi

URL="https://github.com/simdjson/simdjson/releases/download/v4.2.4/singleheader.zip"
if [ "$override" -eq 1 ] || [ ! -d "include/simdjson" ]; then
    rm -rf include/simdjson
    mkdir -p include/simdjson
    curl -L "$URL" -o "include/simdjson/singleheader.zip"
    unzip -q "include/simdjson/singleheader.zip" -d "include/simdjson/"
    rm "include/simdjson/singleheader.zip"
fi

URL="https://github.com/zeux/meshoptimizer/archive/v1.1.zip"
if [ "$override" -eq 1 ] || [ ! -d "include/meshoptimizer" ]; then
    rm -rf include/meshoptimizer
    mkdir -p include/meshoptimizer
    curl -L "$URL" -o "include/meshoptimizer.zip"
    unzip -q "include/meshoptimizer.zip" -d "include/meshoptimizer/"
    mv include/meshoptimizer/meshoptimizer-1.1/* include/meshoptimizer/
    rm -rf include/meshoptimizer/meshoptimizer-1.1
    rm include/meshoptimizer.zip
fi

echo "All dependencies fetched into include/"
