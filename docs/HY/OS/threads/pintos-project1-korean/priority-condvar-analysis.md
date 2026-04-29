# priority-condvar 디버깅/해결 정리 (2026-04-30)

## 전제(코드 기준)
- 기준 소스: `pintos/threads/synch.c`, `pintos/include/threads/synch.h`, `pintos/tests/threads/priority-condvar.c`
- 이 문서는 현재 작업 트리의 구현을 기준으로 작성한다.
- 정합성이 덜한 부분은 명시하고, 확신이 없는 내용은 적지 않거나 별도 주의로 분리한다.

## 핵심 결론 (질문 1, 2에 대한 직접 답)
1) 맞는 부분
- `cond->waiters`에 직접 들어가는 것은 스레드가 아니라 `struct semaphore_elem` 안의 `elem`입니다.  
- `list_entry()`는 `list_elem`에서 **해당 멤버를 포함하고 있는 상위 구조체(컨테이너 구조체 / enclosing struct)**를 되돌리는 용도입니다.
- `waiter.elem` 과 `waiter.semaphore`는 둘이 분리된 객체가 아니라, 같은 `struct semaphore_elem`의 두 멤버(필드)입니다.
- 따라서 “`cond->waiters` 정렬 기준을 바꾸려면 스레드를 직접 넣는 대신 `semaphore_elem` 쪽에서 정렬 키를 들고 있어야 한다”는 접근은 타당합니다.

2) 주의가 필요한 부분
- `sema_down` 안에서 스레드가 `semaphore.waiters`에 들어가는 순간을 이용해 `cond->waiters`를 즉시 다시 정렬하려고 하면, 현재 구현상 `cond_signal` 시점에서 `cond->waiters`의 순서를 안정적으로 재계산하기 어렵습니다.  
- 특히 `semaphore.waiters`는 동기 객체 내부 큐이며, `sema_down`에서는 인터럽트 비활성 구간에서 `thread_current()->elem`을 넣고 바로 블록합니다.  
- 즉, “`cond_wait`에서 넣은 항목의 실제 스레드 우선순위를 그 안에서 직접 추적”하는 방식은 구현 경로가 복잡하고 취약해집니다.

## 구조 정렬

```mermaid
flowchart TB
  subgraph cond_wait()
    A["thread_current()"]
    A --> B["waiter = local struct semaphore_elem"]
    B --> C["waiter.priority = thread_current()->priority"]
    C --> D["cond->waiters에 waiter.elem 삽입 (ordered)"]
    D --> E["lock_release(lock)"]
    E --> F["sema_down(&waiter.semaphore) 실행"]
  end

  subgraph sema_down 내부
    F --> G["while(value == 0)"]
    G --> H["thread_current()->elem을 waiter.semaphore.waiters에 정렬 삽입"]
    H --> I["thread_block()"]
  end

  subgraph cond_signal()
    J["cond_signal(lock)"]
    J --> K["cond->waiters에서 pop_front (가장 높은 우선순위 waiter)"]
    K --> L["sema_up(&popped->semaphore)"]
    L --> M["sema->waiters의 대기 스레드 중 pop_front unblocking"]
    M --> N["해당 스레드가 sema_down에서 복귀 -> lock_reacquire"]
  end
```

## 용어 정리
- `list_elem`:
  - 리스트에 끼우기 위한 노드 포인터(삽입/삭제 대상).
  - 한 객체가 여러 리스트에 동시에 들어갈 수 없고, 상태별로 서로 다른 역할의 `list_elem`만 사용됩니다.
- `컨테이너 구조체(enclosing struct)`:
  - `struct list_entry(e, T, member)` 문법에서 `e`를 포함한 실제 `T` 구조체 포인터를 계산해오는 개념.
  - 즉, `list_entry(e, struct semaphore_elem, elem)`는 `e`가 가리키는 `semaphore_elem` 자체를 의미.
- `현재 구현에서 se의 필드`:
  - 원본 Pintos에서는 `struct semaphore_elem { struct list_elem elem; struct semaphore semaphore; };`
  - 현재 트리에서는 여기에 `int priority;`가 추가되어 우선순위 비교 키로 사용됨.

## 왜 `semaphore`가 “복잡한 통로”가 되는가
- `semaphore`는 단순 정수값이 아니라
  - `value` (현재 토큰 개수)
  - `waiters` (이 세마포어에서 잠자기 대기 중인 스레드들의 리스트)
  를 갖는 동기 객체입니다.
- `sema_down`은 `value==0`이면 `waiters`에 현재 스레드를 넣고 블록, `sema_up`은 `value++` 후 `waiters`에서 하나를 꺾어서 깨웁니다.

## `priority-condvar` 테스트가 요구한 동작 경로
- 테스트는 높은 우선순위 스레드가 먼저 `cond_signal`로 깨워지는 순서를 기대합니다.
- 이 요구를 만족하려면 `cond->waiters`의 pop 대상이 항상 “가장 높은 우선순위 waiter”여야 하므로,
  - `cond_wait`에 들어갈 때 `waiter`가 우선순위를 함께 저장
  - `list_insert_ordered(..., se_priority_greater_comparator)`로 정렬
  - `cond_signal`은 `pop_front`만으로도 우선순위 높은 waiter부터 깨움
  가 성립해야 합니다.

## 정리 (요청에 대한 사실 판정)
- 사용자 문장 중 “맞는 방향”:
  - `cond->waiters` 원소는 스레드가 아니라 `semaphore_elem`이고, `list_entry`로 상위 구조체를 꺼내야 한다.
  - `semaphore_elem`의 필드(`elem`, `semaphore`)는 하나의 객체 안에 있는 분리된 필드이며 분리된 객체가 아니다.
- 사용자 문장 중 **보완 필요**:
  - “`sema_down`에서 등록되는 순간을 이용해 바로 `cond->waiters`를 정렬”은 구현상 불필요하거나 역으로는 비정상적입니다.  
    (`sema_down` 경로는 해당 스레드의 잠금 해제/대기 동작용 내부 상태이며, `cond_wait`의 정렬 책임은 별개로 두는 편이 정합성에 유리.)

## 결론
이 구현에서는 현재 코드처럼 `semaphore_elem`에 `priority`를 저장해 `cond->waiters` 자체를 정렬하는 방식이 핵심이다.  
다만 `semaphore` 쪽도 여전히 “해당 스레드 대기 통로”로서 필요하며, 이 두 구조를 분리해서(컨테이너 정렬 vs 실제 대기 리스트) 이해하면 `priority-condvar`의 동작이 한 번에 보인다.
