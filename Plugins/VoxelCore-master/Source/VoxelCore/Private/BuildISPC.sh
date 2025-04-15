#!/bin/bash

set -e

ISPC="ISPC_PATH"
HEADERS=(ISPC_HEADERS)
ARGS=ISPC_ARGS
INCLUDES=ISPC_INCLUDES

COMMANDS=()

should_compile() {
    local target="$1"
    shift
    local prerequisites=("$@")

    if [[ ! -e "$target" ]]; then
        echo "$target does not exist"
        return 0
    fi

    if [ ! "$(wc -c < "$target")" -gt 16 ]; then
        echo "$target is smaller than 16B"
        return 0
    fi

    local target_time=$(stat -c %Y "$target")
    for prereq in "${prerequisites[@]}"; do
        local prereq_time=$(stat -c %Y "$prereq")
        if (( prereq_time > target_time )); then
            echo "$target is older than $prereq"
            return 0
        fi
    done

    # echo "$(basename "$target") is up to date"
    return 1
}

build() {
    local source_file="$1"
    local object_file="$2"
    local generated_header="$3"

    local prerequisites=("${HEADERS[@]}" "$source_file")

    if should_compile "$generated_header" "${prerequisites[@]}"; then
        COMMANDS+=("$ISPC \"$source_file\" -h \"$generated_header\" -DVOXEL_DEBUG=0 --werror $INCLUDES $ARGS")
    fi

    if should_compile "$object_file" "${prerequisites[@]}"; then
        COMMANDS+=("$ISPC \"$source_file\" -o \"$object_file\" -O3 -DVOXEL_DEBUG=0 --werror $INCLUDES $ARGS")
    fi
}

build_library() {
    local target_file="$1"
    shift
    local object_files=("$@")

    if should_compile "$target_file" "${object_files[@]}"; then
        rm -f "$target_file"
        COMMANDS+=("ar rcs \"$target_file\" $(printf '"%s" ' "${object_files[@]}")")
    fi
}

flush_commands() {
    local local_commands=()
    for command in "${COMMANDS[@]}"; do
      local_commands+=("$command || { echo \"Error: Command failed: $command\"; exit 1; }")
    done

    printf "%s\0" "${local_commands[@]}" > commands.txt
    xargs -0 -S131072 -I {} -P 16 bash -c '{}' < commands.txt

    COMMANDS=()
}
