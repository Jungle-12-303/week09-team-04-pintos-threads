---
id: pintos-project1-alarm-clock
category: OS
topic: Pintos
subtopic: Threads Project 1
level: 2
difficulty: medium
status: completed
updated: 2026-04-24
tags: [pintos, alarm-clock, timer, threads]
prereq: [pintos-project1-intro]
source: "pintos_threads_concepts"
---

# Project 1-1: Alarm Clock

## 상위/관련 링크 (필수)
- 상위: [OS 인덱스](../README.md)
- 관련: [알람 시계 원문과 연결](./01-introduction-and-overview.md), [우선순위 스케줄링](./03-priority-scheduling.md)

## 과제 목표

`devices/timer.c`에 있는 `timer_sleep()`을 다시 구현한다.

기존 구현은 **busy waiting(바쁜 대기)** 형태라,
시간이 지날 때까지 루프를 돌고 `thread_yield()`를 반복한다.  
이를 제거하고 비차단 방식으로 바꾼다.

## 함수 정의

```c
void timer_sleep (int64_t ticks);
```

## 의미

호출한 스레드를 최소 `ticks`만큼의 타이머 tick 단위로 대기시킨다.  
시스템이 유휴 상태가 아니라면, 정확히 `ticks`가 지난 시점과 정확히 일치해 깨어날 필요는 없다.

필수: 대기 시간이 끝나면 준비 큐(ready queue)에 다시 넣는다.

## 단위

- 인수 단위는 타이머 tick이다.
- 밀리초/마이크로초/나노초가 아니다.
- `devices/timer.h`의 `TIMER_FREQ`가 초당 tick 수를 정의한다.
- 기본값은 100(초당 100 tick)이며 변경하지 않는 것을 권장한다.
  - 변경 시 테스트 실패 가능성 큼.

## 보조 함수

`timer_msleep()`, `timer_usleep()`, `timer_nsleep()`가 각각 ms/µs/ns 기반 인터페이스를 제공한다.  
필요 시 자동으로 `timer_sleep()`를 호출하므로 직접 수정할 필요는 없다.

## 추가

이 알람 시계 구현은 이후 프로젝트에서는 직접 필수는 아니지만,  
project 4에서 쓸 수 있는 유용한 개념이다.

