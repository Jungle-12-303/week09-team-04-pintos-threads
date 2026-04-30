# priority-condvar 디버깅/해결 정리 (2026-04-30)

## 전제(코드 기준)
- 기준 소스: `pintos/threads/synch.c`, `pintos/include/threads/synch.h`, `pintos/tests/threads/priority-condvar.c`
- 이 문서는 현재 작업 트리의 구현을 기준으로 작성한다.
- 정합성이 덜한 부분은 명시하고, 확신이 없는 내용은 적지 않거나 별도 주의로 분리한다.

## 핵심 결론
1. `cond->waiters`에 직접 들어가는 것은 스레드가 아니라 `struct semaphore_elem` 안의 `elem`이다.
2. `list_entry()`는 `list_elem`에서 그것을 포함하는 상위 구조체, 즉 컨테이너 구조체를 되찾는 용도다.
3. `waiter.elem`과 `waiter.semaphore`는 따로 노는 두 객체가 아니라, 같은 `struct semaphore_elem` 안에 들어 있는 두 멤버다.
4. 따라서 `cond->waiters`를 우선순위로 정렬하려면, 정렬 기준을 `semaphore_elem` 쪽에서 바로 꺼낼 수 있어야 한다.

## 구조 그림

![priority-condvar 구조도](/abs/path/D:\03Dev\05Jungle\Pintos\pintos_22.04_lab_docker\docs\HY\resources\materials\week9_pintos_threads_concepts\priority-condvar-structure.svg)

![semaphore_elem 중심 구조도](/abs/path/D:\03Dev\05Jungle\Pintos\pintos_22.04_lab_docker\docs\HY\resources\materials\week9_pintos_threads_concepts\priority-condvar-semaphore-elem-focus.svg)

## 그림으로 보는 핵심 해석
- `cond_wait()`에 들어오면 지역 변수 `waiter`가 만들어진다.
- `cond->waiters`에는 `&waiter.elem`이 들어간다.
- 하지만 실제로 잠드는 현재 스레드는 `sema_down(&waiter.semaphore)`를 통해 `waiter.semaphore.waiters`에 들어간다.
- 즉 `cond->waiters`와 `waiter.semaphore.waiters`는 같은 리스트가 아니고, 역할도 다르다.

## 두 번째 그림에서 특히 봐야 할 점
- `elem`은 `cond->waiters`의 원소로 들어가는 리스트 노드다.
- `priority`는 그 `elem`이 `cond->waiters` 안에서 어디에 들어갈지 결정하는 정렬 키다.
- `semaphore.waiters`에는 실제로 블록된 스레드의 `thread.elem`이 들어간다.
- 한 번의 `cond_wait()` 호출마다 `waiter` 하나가 생기고, 그 `waiter.semaphore`로 잠드는 현재 스레드도 하나이므로, 이 문맥에서는 `waiter`와 대기 스레드가 사실상 1:1로 대응한다고 볼 수 있다.

## 왜 sema_down 시점에서 cond->waiters 정렬을 풀기 어려운가
- `sema_down()`은 세마포어 내부 대기 큐에 현재 스레드를 넣고 블록시키는 경로다.
- 이 경로는 “실제로 잠들고 깨우는 통로”이지, 조건변수 리스트 자체의 정렬 기준을 관리하는 주체로 보기 어렵다.
- 따라서 `cond->waiters`를 정렬해야 하는 책임을 `sema_down()` 내부에 기대면 구조가 복잡해지고 설명도 불안정해진다.

## 그래서 현재 접근이 왜 맞는가
- `cond->waiters`는 `semaphore_elem`을 담는 리스트다.
- 그러면 가장 단순하고 안전한 방법은 `semaphore_elem`에 우선순위 필드를 두고 그 값을 기준으로 정렬하는 것이다.
- 현재 코드의 `waiter.priority = thread_current()->priority;`와 `list_insert_ordered(&cond->waiters, &waiter.elem, se_priority_greater_comparator, NULL);`는 바로 그 역할을 한다.

## 용어 정리
- `list_elem`: 리스트에 연결되기 위한 노드 멤버
- `컨테이너 구조체(enclosing struct)`: 어떤 멤버를 포함하고 있는 바깥 구조체
- `list_entry(e, struct semaphore_elem, elem)`: `e`가 가리키는 `elem`이 들어 있는 `struct semaphore_elem`를 복원하는 표현

## 사실 판정
- 맞는 설명:
  - `cond->waiters`에는 스레드가 아니라 `semaphore_elem` 쪽의 `elem`이 저장된다.
  - `list_entry`를 통해 `elem`에서 상위 구조체인 `semaphore_elem`에 접근할 수 있다.
  - `semaphore_elem` 안의 `semaphore`를 통해 내부 `waiters` 리스트에 접근할 수 있다.
- 보완이 필요한 설명:
  - “`sema_down`에서 현재 스레드가 저장되는 순간을 이용해 `cond->waiters`를 정렬한다”는 표현은 구조상 핵심 해법이라고 보기 어렵다.
  - 더 정확한 설명은 “`cond->waiters`가 `semaphore_elem` 리스트이므로, 그 구조체 안에 우선순위를 저장해서 정렬한다”이다.

## 결론
`priority-condvar`를 통과시키는 핵심은 `cond->waiters`에서 무엇이 정렬 대상인지 정확히 구분하는 것이다.  
정렬 대상은 스레드가 아니라 `semaphore_elem`이고, 실제 수면/기상 통로는 그 안의 `semaphore.waiters`이므로, `semaphore_elem`에 우선순위를 보관해 정렬하는 접근이 가장 직접적이고 설명도 깔끔하다.
