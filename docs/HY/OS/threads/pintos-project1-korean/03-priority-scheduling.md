---
id: pintos-project1-priority-scheduling
category: OS
topic: Pintos
subtopic: Threads Project 1
level: 3
difficulty: high
status: completed
updated: 2026-04-24
tags: [pintos, scheduler, priority, donation, thread]
prereq: [pintos-project1-alarm-clock]
source: "pintos_threads_concepts"
---

# Project 1-2: Priority Scheduling

## 상위/관련 링크 (필수)
- 상위: [OS 인덱스](../README.md)
- 관련: [알람 시계](./02-alarm-clock.md), [고급 스케줄러(MLFQS)](./04-advanced-scheduler.md)

## 과제 목표

Pintos에 우선순위 스케줄링과 우선순위 기부(priority donation)를 구현한다.

## 핵심 규칙

1. 준비 큐에 더 높은 우선순위 스레드가 추가되면, 현재 실행 스레드는 즉시 양보하여 CPU를 내놓아야 한다.
2. 락/세마포어/조건변수 대기열에서도 **가장 높은 우선순위** 스레드가 먼저 깨어나야 한다.
3. 스레드는 자기 우선순위를 언제든 올리거나 내릴 수 있다.
4. 내림으로 인해 최고 우선순위가 사라지는 경우 즉시 CPU를 양보한다.

## 우선순위 범위

- 범위: `PRI_MIN(0)` ~ `PRI_MAX(63)`
- 숫자가 클수록 높은 우선순위.
- 초기 우선순위는 `thread_create()` 인수로 전달.
- 기본값은 `PRI_DEFAULT(31)` 사용 권장.
- 우선순위 상수는 `threads/thread.h`에 고정, 변경 금지.

## 우선순위 역전(Priority Inversion) 문제

시나리오: `H`(높음), `M`(중간), `L`(낮음)  
`H`가 `L`이 잡고 있는 락을 기다리고, `M`이 준비 큐에 있다면:
- `H`는 CPU를 못 받음
- `L`은 스케줄링 기회가 부족함
- 결과적으로 `H`는 영원히 대기

이를 막는 핵심이 **우선순위 기부**이다.

`H`가 `L`이 락을 놓을 때까지 기다리는 동안,
`H`의 우선순위를 `L`이 임시로 받아서 `L`을 먼저 수행하게 한다.
락이 반환되면 다시 원래 우선순위로 돌아간다.

## 기부 구현 범위

필수 구현: 락에서의 우선순위 기부(여러 번 중첩 포함).  
다른 동기화 객체에는 필수 아님.

- 다중 기부 처리: 여러 스레드가 한 스레드에 기부하는 상황
- 중첩 기부 처리: H waits on L1 held by M, M waits on L2 held by L이면 H의 우선순위를 M, L 둘 다 반영
- 깊이 제한: 필요 시 합리적 제한(예: 최대 8단계)을 둘 수 있음

우선순위 스케줄링 자체는 모든 경우에서 적용되어야 한다.

## 인터페이스 구현

다음 함수 구현 필요:

```c
void thread_set_priority (int new_priority);
```

- 현재 스레드의 우선순위를 `new_priority`로 설정
- 현재 스레드가 최고 우선순위를 잃으면 즉시 yield

```c
int thread_get_priority (void);
```

- 현재 스레드의 우선순위를 반환
- 우선순위 기부가 적용된 상태면 “더 높은(기부된)” 값 반환

다른 스레드 우선순도를 직접 수정하는 인터페이스는 만들지 않는다.

## 메모

- 이 과제의 우선순위 스케줄러는 이후 프로젝트에 직접 연계되지 않는다.

