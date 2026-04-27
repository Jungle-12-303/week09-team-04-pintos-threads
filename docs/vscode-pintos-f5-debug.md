# VS Code Pintos F5 Debugging

이 저장소는 VS Code Dev Container 안에서 `F5` 한 번으로 아래 순서를 실행하도록 설정되어 있습니다.

1. 선택한 Pintos 프로젝트(`threads`, `userprog`, `vm`)를 `make`
2. `pintos --gdb`로 QEMU를 정지 상태(`-S`)로 실행
3. VS Code C/C++ 확장이 GDB로 `127.0.0.1:1234`에 attach

## 1. 한 번만 할 일

1. VS Code에서 저장소를 엽니다.
2. `Dev Containers: Rebuild and Reopen in Container`를 실행합니다.
3. 컨테이너가 다시 열리면 터미널에서 아래 두 줄을 확인합니다.

```bash
command -v pintos
command -v gdb
```

정상이면 `pintos`는 `/workspaces/<현재-저장소>/pintos/utils/pintos` 쪽 경로로 잡히고, `gdb`는 `/usr/bin/gdb`로 보입니다.

## 2. F5로 디버깅하는 방법

1. 디버깅하려는 소스에 브레이크포인트를 겁니다.
2. `Run and Debug`에서 아래 셋 중 하나를 고릅니다.
   - `Pintos: threads`
   - `Pintos: userprog`
   - `Pintos: vm`
3. `F5`를 누릅니다.
4. 실행 직전에 Pintos 인자를 묻는 창이 뜨면 기본값을 쓰거나 원하는 테스트로 바꿉니다.
5. 빌드가 끝나고 `PINTOS_DEBUG_READY ...`가 보이면 GDB가 자동으로 붙고 실행이 이어집니다.

기본 입력값은 다음처럼 잡혀 있습니다.

- `threads`: `-v -- -q run alarm-single`
- `userprog`: `-p tests/userprog/halt -a halt -- -q run halt`
- `vm`: `-p tests/vm/pt-grow-stack -a pt-grow-stack -- -q run pt-grow-stack`

다른 테스트를 쓰고 싶으면 입력창에서 그대로 바꿔 넣으면 됩니다. 인자는 셸 스타일 인용부호를 지원합니다.

## 3. 자주 쓰는 예시 인자

```bash
# threads
-v -- -q run priority-donate-one

# userprog
-p tests/userprog/args-many -a args-many -- -q run args-many

# vm
-p tests/vm/pt-big-stk-obj -a pt-big-stk-obj -- -q run pt-big-stk-obj
```

## 4. 파일 위치

- 디버그 구성: `.vscode/launch.json`
- 태스크 구성: `.vscode/tasks.json`
- 실행 스크립트: `scripts/pintos-debug-session.sh`

## 5. 브레이크포인트가 안 먹기 쉬운 원인

1. `threads`, `userprog`, `vm` 중 잘못된 구성을 선택한 경우
   같은 파일명이라도 어떤 커널을 빌드했는지에 따라 심볼이 달라집니다.

2. 소스는 바꿨는데 직전 `make`가 실패한 경우
   이때는 예전 `kernel.o` 심볼로 GDB가 붙어서 브레이크포인트가 어긋날 수 있습니다.

3. 브레이크포인트를 사용자 프로그램 소스에 건 경우
   현재 F5 구성은 `kernel.o` 심볼 기준의 커널 디버깅입니다. `tests/userprog/*.c`나 `tests/vm/*.c` 자체에 직접 브레이크포인트를 거는 용도는 아닙니다.

4. 너무 이른 부트 코드에 브레이크포인트를 건 경우
   QEMU는 GDB attach 후 자동으로 continue 됩니다. 초반 부트 경로를 꼭 잡아야 하면 미리 브레이크포인트를 걸어 두고 시작하거나, attach 직후 일시정지해서 진행하세요.

5. 같은 포트의 이전 QEMU 세션이 남아 있던 경우
   이 스크립트는 새 디버그를 시작할 때 기존 Pintos 디버그 세션을 먼저 정리합니다. 그래도 이상하면 `Terminal`에서 `Tasks: Run Task` -> `pintos: stop debug session`을 한 번 실행한 뒤 다시 `F5`를 누르세요.

## 6. 참고

- 이 설정은 한 번에 하나의 Pintos 디버그 세션만 쓰는 전제를 둡니다. GDB 포트는 `1234`입니다.
- 디버깅을 끝내면 `postDebugTask`가 자동으로 QEMU를 정리합니다.
- 일반 터미널과 VS Code task 모두 현재 워크스페이스 기준으로 `pintos` 명령을 바로 쓸 수 있게 Dev Container 환경이 맞춰져 있습니다.
