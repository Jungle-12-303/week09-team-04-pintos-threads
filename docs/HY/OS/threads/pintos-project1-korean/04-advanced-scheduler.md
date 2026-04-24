45565---
id: pintos-project1-advanced-scheduler
category: OS
topic: Pintos
subtopic: Threads Project 1
level: 4
difficulty: high
status: completed
updated: 2026-04-24
tags: [pintos, mlfqs, scheduler, niceness, load-average, fixed-point]
prereq: [pintos-project1-priority-scheduling]
source: "pintos_threads_concepts"
---

# Project 1-3: Advanced Scheduler (MLFQS)

## 상위/관련 링크 (필수)
- 상위: [OS 인덱스](../README.md)
- 관련: [우선순위 스케줄링](./03-priority-scheduling.md), [고정소수점 계산](./05-fixed-point-arithmetic.md)

## 과제 목표

4.4BSD의 다단계 피드백 큐 스케줄링(MLFQS)으로 확장해 평균 응답시간을 개선한다.

## 기본 정책

- 우선순위 스케줄러와 마찬가지로 우선순위 기반 선택
- 다만 **우선순위 기부는 없음**
- 기본값: 기존 우선순위 스케줄러 활성
- 부팅 옵션 `-mlfqs`로 고급 스케줄러 선택

`-mlfqs`가 전달되면 `parse_options()`가 `thread_mlfqs`를 true로 설정한다.

MLFQS 모드에서:
- `thread_create()` 우선순위 인수 무시
- `thread_set_priority()`는 의미 없음
- `thread_get_priority()`는 스케줄러가 계산한 현재 우선순위 반환

## 4.4BSD 스케줄러 개요

목표는 다양한 작업 특성을 조합해 공평성과 반응성 확보.

- I/O 중심 스레드: 짧은 응답 필요, CPU 사용량은 적음
- CPU 중심 스레드: 많은 CPU 필요, 응답성은 덜 중요
- 실제 워크로드는 중간 형태가 많음

MLFQS는 여러 개 준비 큐를 사용한다:
- 큐마다 서로 다른 우선순위
- 최고 우선순위 비어있지 않은 큐에서 먼저 스케줄
- 같은 큐에서는 라운드 로빈

## 업데이트 시점 규칙

일부 값은 tick 단위 타이머 동기화가 필요하다.
반드시 일반 커널 스레드가 실행되기 전에 갱신되어야 하며,
갱신 중간에 오래된/새 값 혼재가 생기면 안 된다.

## Nice 값

- 각 스레드가 가질 수 있는 nice 범위: -20 ~ 20
- `nice = 0`: 우선순위 영향 없음
- 양수 nice: CPU 점유를 덜 받음(우선순위 하락 경향)
- 음수 nice: CPU 점유를 더 받음(우선순위 상승 경향)
- 초기 스레드의 nice는 0
- 자식 스레드는 부모 nice를 상속

함수:

```c
int thread_get_nice(void);
```
- 현재 스레드의 nice 반환

```c
int thread_set_nice(int nice);
```
- nice 갱신 후 우선순위 재계산
- 실행 중 스레드의 우선순위가 최고가 아니면 즉시 yield

## 우선순위 계산식

Pintos는 총 64개 우선순위를 사용한다.  

```
priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)
```

- `recent_cpu`: 최근 사용 CPU 추정치
- `nice`: 스레드 nice
- 나눗셈은 버림(truncation)
- 결과는 [PRI_MIN, PRI_MAX] 범위로 클램프

우선순위가 낮다는 것은 바로 직전 CPU를 많이 쓴 스레드를 상대적으로 늦추어, CPU를 덜 받은 스레드가 먼저 돌아가게 만드는 장치다.

## `recent_cpu` 계산

각 tick마다 실행 중인 스레드의 `recent_cpu` +1 (idle 제외)

매초마다 모든 스레드에 대해:

```
recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice
```

갱신 시점은 엄격히 `timer_ticks() % TIMER_FREQ == 0` 이어야 하며, 다른 시점에서는 갱신하면 안 된다.

`nice`가 음수이면 `recent_cpu`도 음수가 될 수 있으므로 0으로 강제 클램프하면 안 된다.

### 계산 순서 주의
`load_avg * recent_cpu`를 먼저 곱하면 오버플로우 위험이 있어,
계수 계산 후 곱셈을 권장한다.

## `load_avg` 계산

시스템 전체 실행대기 추정치로 분당 스레드 준비 상태를 반영.

초기값: 0
매초 갱신:

```
load_avg = (59/60) * load_avg + (1/60) * ready_threads
```

`ready_threads`는 실행 중 또는 준비 상태 스레드 수(idle 제외).

갱신 시점 역시 `timer_ticks() % TIMER_FREQ == 0`이 필수.

함수:

```c
int thread_get_load_avg(void);
```
- 현재 시스템 load_avg의 100배 정수 반올림 값을 반환

## 요약

매 4틱마다 우선순위 재계산  
매초 `recent_cpu`와 `load_avg` 재계산  
수치들의 실수 계산은 고정소수점으로 처리

