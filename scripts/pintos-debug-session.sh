#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
PINTOS_ROOT="${REPO_ROOT}/pintos"
DEFAULT_GDB_PORT="${PINTOS_GDB_PORT:-1234}"
PROJECTS=(threads userprog vm)

usage() {
    cat <<'EOF'
Usage:
  scripts/pintos-debug-session.sh start <threads|userprog|vm>
  scripts/pintos-debug-session.sh stop <threads|userprog|vm|all>
EOF
}

ensure_project() {
    local project="$1"

    case "${project}" in
        threads|userprog|vm)
            ;;
        *)
            echo "Unknown Pintos project: ${project}" >&2
            usage >&2
            exit 1
            ;;
    esac
}

project_dir() {
    printf '%s/%s' "${PINTOS_ROOT}" "$1"
}

build_dir() {
    printf '%s/build' "$(project_dir "$1")"
}

pid_file() {
    printf '%s/.pintos-debug.pid' "$(build_dir "$1")"
}

read_pintos_args() {
    python3 - <<'PY'
import os
import shlex

for arg in shlex.split(os.environ.get("PINTOS_DEBUG_ARGS", "")):
    print(arg)
PY
}

port_is_open() {
    local port="$1"

    python3 - "$port" <<'PY'
import socket
import sys

port = int(sys.argv[1])

with socket.socket() as sock:
    sock.settimeout(0.2)
    try:
        sock.connect(("127.0.0.1", port))
    except OSError:
        raise SystemExit(1)

raise SystemExit(0)
PY
}

session_pid_matches() {
    local pid="$1"
    local args

    args="$(ps -o args= -p "${pid}" 2>/dev/null || true)"
    [[ -n "${args}" && "${args}" == *"/pintos/utils/pintos"* ]]
}

stop_one() {
    local project="$1"
    local session_pid
    local session_pid_file

    session_pid_file="$(pid_file "${project}")"
    if [[ ! -f "${session_pid_file}" ]]; then
        return 0
    fi

    session_pid="$(<"${session_pid_file}")"
    if [[ -n "${session_pid}" ]] && kill -0 "${session_pid}" 2>/dev/null; then
        if session_pid_matches "${session_pid}"; then
            kill -TERM -- "-${session_pid}" 2>/dev/null || kill -TERM "${session_pid}" 2>/dev/null || true
            for _ in $(seq 1 25); do
                if ! kill -0 "${session_pid}" 2>/dev/null; then
                    break
                fi
                sleep 0.2
            done

            if kill -0 "${session_pid}" 2>/dev/null; then
                kill -KILL -- "-${session_pid}" 2>/dev/null || kill -KILL "${session_pid}" 2>/dev/null || true
            fi
        fi
    fi

    rm -f "${session_pid_file}"
}

stop_all() {
    local project
    for project in "${PROJECTS[@]}"; do
        stop_one "${project}"
    done
}

start_session() {
    local project="$1"
    local session_pid
    local session_pid_file
    local project_build_dir
    local -a pintos_args=()

    ensure_project "${project}"
    stop_all

    if [[ -f "${PINTOS_ROOT}/activate" ]]; then
        # shellcheck disable=SC1091
        source "${PINTOS_ROOT}/activate"
    fi

    if ! command -v pintos >/dev/null 2>&1; then
        echo "pintos command not found. Rebuild or reopen the Dev Container first." >&2
        exit 1
    fi

    project_build_dir="$(build_dir "${project}")"
    session_pid_file="$(pid_file "${project}")"

    if [[ ! -d "${project_build_dir}" ]]; then
        echo "Missing build directory: ${project_build_dir}" >&2
        echo "Run make in pintos/${project} first." >&2
        exit 1
    fi

    mapfile -t pintos_args < <(read_pintos_args)

    cd "${project_build_dir}"
    echo "PINTOS_DEBUG_START ${project} ${DEFAULT_GDB_PORT}"

    setsid pintos --gdb "${pintos_args[@]}" &
    session_pid="$!"
    printf '%s\n' "${session_pid}" > "${session_pid_file}"
    trap "rm -f -- '${session_pid_file}'" EXIT

    for _ in $(seq 1 150); do
        if port_is_open "${DEFAULT_GDB_PORT}"; then
            echo "PINTOS_DEBUG_READY ${project} ${DEFAULT_GDB_PORT}"
            wait "${session_pid}"
            return $?
        fi

        if ! kill -0 "${session_pid}" 2>/dev/null; then
            wait "${session_pid}" || true
            echo "PINTOS_DEBUG_FAILED ${project} ${DEFAULT_GDB_PORT}" >&2
            return 1
        fi

        sleep 0.2
    done

    echo "Timed out waiting for the Pintos GDB server on port ${DEFAULT_GDB_PORT}." >&2
    stop_one "${project}"
    return 1
}

main() {
    if [[ $# -lt 2 ]]; then
        usage >&2
        exit 1
    fi

    local action="$1"
    local target="$2"

    case "${action}" in
        start)
            start_session "${target}"
            ;;
        stop)
            if [[ "${target}" == "all" ]]; then
                stop_all
            else
                ensure_project "${target}"
                stop_one "${target}"
            fi
            ;;
        *)
            usage >&2
            exit 1
            ;;
    esac
}

main "$@"
