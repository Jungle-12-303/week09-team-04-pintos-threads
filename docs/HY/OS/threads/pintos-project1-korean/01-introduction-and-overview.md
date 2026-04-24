---
id: pintos-project1-intro
category: OS
topic: Pintos
subtopic: Threads Project 1
level: 2
difficulty: medium
status: completed
updated: 2026-04-24
tags: [pintos, threads, overview, source-files, synchronization, development]
prereq: []
next: [pintos-project1-alarm-clock, pintos-project1-priority-scheduling, pintos-project1-advanced-scheduler]
source: "pintos_threads_concepts"
---

# Project 1: Threads — 소개 및 개요

## 상위/관련 링크 (필수)
- 상위: [OS 인덱스](../README.md)
- 관련: [알람 시계 과제](./02-alarm-clock.md), [우선순위 스케줄링](./03-priority-scheduling.md), [고급 스케줄러](./04-advanced-scheduler.md)

## 1) 소개

이 과제에서는 최소 기능을 갖춘 스레드 시스템을 제공한다.  
너의 작업은 이 시스템을 확장해 동기화 문제를 이해하고 해결하는 것이다.

주요 작업 디렉터리는 `threads`이며, 일부는 `devices`도 필요하다.  
컴파일은 `threads` 디렉터리에서 수행한다.

## 2) 배경: 스레드 이해

초기 스레드 시스템은 이미 다음을 구현한다.

- 스레드 생성 및 완료
- 간단한 스케줄러(스레드 전환)
- 동기화 프리미티브: 세마포어, 락, 조건변수, 최적화 배리어

아직 기본 시스템을 직접 컴파일/실행하지 않았다면, 먼저 해보는 것을 권장한다.  
원하면 소스에서 필요한 지점에 `printf()`를 넣고 재컴파일 후 동작 순서를 확인할 수 있다.

`thread_create()`로 새 스레드 생성 시:
- 실행 컨텍스트를 새로 만든다.
- 전달한 함수가 그 스레드의 진입점이 된다.
- 함수가 반환하면 스레드 종료.

시점 별로 한 개의 스레드만 실행되고, 나머지는 비활성 상태가 된다.  
실행 가능한 스레드가 없으면 `idle()`의 idle 스레드가 동작한다.

동기화 프리미티브는 한 스레드가 다른 스레드의 수행을 기다릴 때 컨텍스트 스위치 유발한다.

## 3) 컨텍스트 스위치 동작

`threads/thread.c`의 `thread_launch()`에서 동작한다(완전한 이해는 필수 아님).

- 현재 스레드 상태 저장
- 대상 스레드 상태 복원

GDB에서 `schedule()`에 브레이크포인트를 찍고 step 실행하면  
`do_iret()`에서 `iret` 실행 시 다른 스레드가 실행되는 흐름을 볼 수 있다.

## 4) 스택 오버플로우 경고

각 스레드 스택은 약 4KB의 작은 고정 크기다.  
커널은 오버플로우를 탐지하려 하지만 완벽하지 않다.

`int buf[1000];` 같은 큰 지역 배열을 스택에 올리면 의도치 않은 패닉이 생길 수 있다.  
대체로 `page allocator` 또는 `block allocator`를 사용한다.

## 4.1) 소스 파일 개요 (필독 가이드)

### threads / include/threads

- `loader.S`, `loader.h`: 부트로더 (대부분 수정 불필요)
- `kernel.lds.S`: 링크 스크립트 (수정 불필요)
- `init.c`, `init.h`: 커널 초기화
- `thread.c`, `thread.h`: 기본 스레드 지원(가장 많이 수정)
- `palloc.c`, `palloc.h`: 페이지 할당자 (4KB 단위)
- `malloc.c`, `malloc.h`: 커널용 malloc/free
- `interrupt.c`, `interrupt.h`: 인터럽트 기본 처리
- `intr-stubs.S`, `intr-stubs.h`: 저수준 인터럽트 어셈블리
- `synch.c`, `synch.h`: 동기화 기본 요소
- `mmu.c`, `mmu.h`: x86-64 페이지 테이블
- `io.h`: I/O 포트 접근
- `vaddr.h`, `pte.h`: 가상주소/페이지 엔트리
- `flags.h`: x86-64 플래그 비트 매크로

### devices

- `timer.c`, `timer.h`: 시스템 타이머(기본 100Hz), 이번 과제 핵심 수정
- `vga.c`, `vga.h`: VGA 텍스트 출력
- `serial.c`, `serial.h`: 시리얼 드라이버
- `block.c`, `block.h`: 블록 장치 추상화
- `ide.c`, `ide.h`: IDE 디스크
- `partition.c`, `partition.h`: 파티션 관리
- `kbd.c`, `kbd.h`: 키보드
- `input.c`, `input.h`: 입력 레이어
- `intq.c`, `intq.h`: 인터럽트 큐
- `rtc.c`, `rtc.h`: 실시간 시각
- `speaker.c`, `speaker.h`: 스피커
- `pit.c`, `pit.h`: 8254 PIT 설정

### lib / lib/kernel

- 기본 C 라이브러리 헤더/함수: `ctype`, `stdio`, `stdlib`, `string` 계열
- `debug.c`, `debug.h`: 디버깅 지원
- `random.c`, `random.h`: 난수 생성
- `round.h`: 반올림 관련 매크로
- `syscall-nr.h`: 시스템콜 번호(과제2부터)
- `kernel/list.*`: 이중 연결 리스트(과제1에서 적극 권장)
- `kernel/bitmap.*`: 비트맵
- `kernel/hash.*`: 해시 테이블
- `kernel/console.*`, `kernel/stdio.h`: 출력 구현

## 5) 동기화(Synchronization) 핵심

인터럽트를 끄면 동시성이 사라져 레이스를 피할 수 있어 단순하게 보이지만,  
모든 동기화 문제를 그렇게 푸는 건 바람직하지 않다.

일반적으로는:
- 세마포어
- 락
- 조건변수  
를 사용한다.

### 인터럽트 비활성화가 필요한 유일한 대표 케이스

커널 스레드와 인터럽트 핸들러 간 공유 데이터 접근은 인터럽트를 끄는 편이 필요하다.  
인터럽트 핸들러는 sleep할 수 없기 때문에 락을 사용할 수 없다.

예:
- alarm clock에서 타이머 인터럽트가 잠자는 스레드를 깨우는 경우
- 고급 스케줄러에서 전역/스레드별 변수 접근

이 경우 커널 스레드 측에서 인터럽트를 잠깐 끄지 않으면 간섭이 생길 수 있다.

주의: 인터럽트 비활성화 구간을 길게 두면 타이머 틱이나 입력 이벤트 손실이 생길 수 있다.  
동기화는 최소 구간에서만 수행한다.

`synch.c` 내부 구현도 내부적으로 인터럽트 비활성화를 사용한다. 필요한 경우 늘릴 수 있으나 최소화가 원칙이다.

디버깅용으로 잠깐 쓰는 건 가능하지만 제출 전엔 반드시 제거한다.

`thread_yield()`를 반복 호출하는 형태의 바쁜 대기(Busy Waiting)는 제출에서 허용되지 않는다.

## 6) 개발 제안

- 과제를 “개인별로 분업 후 나중에 합치기”는 충돌 위험이 크다.
- git 등 버전 관리로 **작업을 자주 통합**하는 방식 권장.
- 버그가 날 경우 이전 커밋으로 되돌릴 수 있게 하는 것이 안정적.

버그가 난다면 `Debugging Tools`를 다시 읽고, 특히 백트레이스(backtrace) 분석을 활용해  
커널 패닉/단언 실패의 핵심 원인을 빠르게 찾는 습관이 중요하다.

