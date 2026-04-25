#include "list.h"
#include "../debug.h"

/* Our doubly linked lists have two header elements: the "head"
   just before the first element and the "tail" just after the
   last element.  The `prev' link of the front header is null, as
   is the `next' link of the back header.  Their other two links
   point toward each other via the interior elements of the list.

   An empty list looks like this:

   +------+     +------+
   <---| head |<--->| tail |--->
   +------+     +------+

   A list with two elements in it looks like this:

   +------+     +-------+     +-------+     +------+
   <---| head |<--->|   1   |<--->|   2   |<--->| tail |<--->
   +------+     +-------+     +-------+     +------+

   The symmetry of this arrangement eliminates lots of special
   cases in list processing.  For example, take a look at
   list_remove(): it takes only two pointer assignments and no
   conditionals.  That's a lot simpler than the code would be
   without header elements.

   (Because only one of the pointers in each header element is used,
   we could in fact combine them into a single header element
   without sacrificing this simplicity.  But using two separate
   elements allows us to do a little bit of checking on some
   operations, which can be valuable.)

   이 이중 연결 리스트는 두 개의 헤더 원소를 가진다. 첫 번째 원소 바로
   앞에는 "head"가 있고, 마지막 원소 바로 뒤에는 "tail"이 있다. 앞쪽
   헤더의 `prev` 링크는 null이고, 뒤쪽 헤더의 `next` 링크도 null이다.
   나머지 두 링크는 리스트 내부 원소들을 거쳐 서로를 향한다.

   빈 리스트는 다음과 같은 형태다:

   +------+     +------+
   <---| head |<--->| tail |--->
   +------+     +------+

   두 원소가 들어 있는 리스트는 다음과 같은 형태다:

   +------+     +-------+     +-------+     +------+
   <---| head |<--->|   1   |<--->|   2   |<--->| tail |<--->
   +------+     +-------+     +-------+     +------+

   이런 대칭적인 구조 덕분에 리스트 처리에서 많은 특수 사례가 사라진다.
   예를 들어 list_remove()를 보면 포인터 대입 두 번만 필요하고 조건문은
   필요 없다. 헤더 원소가 없을 때보다 훨씬 단순하다.

   각 헤더 원소에서는 포인터 하나만 사용하므로, 사실 이 둘을 하나의 헤더
   원소로 합쳐도 단순성은 유지할 수 있다. 하지만 두 개의 별도 원소를
   사용하면 몇몇 연산에서 약간의 검사를 할 수 있고, 이것이 유용할 수 있다. */

static bool is_sorted (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) UNUSED;

/* Returns true if ELEM is a head, false otherwise.
   ELEM이 head이면 true를, 아니면 false를 반환한다. */
static inline bool
is_head (struct list_elem *elem) {
	return elem != NULL && elem->prev == NULL && elem->next != NULL;
}

/* Returns true if ELEM is an interior element,
   false otherwise.
   ELEM이 내부 원소이면 true를, 아니면 false를 반환한다. */
static inline bool
is_interior (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next != NULL;
}

/* Returns true if ELEM is a tail, false otherwise.
   ELEM이 tail이면 true를, 아니면 false를 반환한다. */
static inline bool
is_tail (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next == NULL;
}

/* Initializes LIST as an empty list.
   LIST를 빈 리스트로 초기화한다. */
void
list_init (struct list *list) {
	ASSERT (list != NULL);
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}

/* Returns the beginning of LIST.
   LIST의 시작 원소를 반환한다. */
struct list_elem *
list_begin (struct list *list) {
	ASSERT (list != NULL);
	return list->head.next;
}

/* Returns the element after ELEM in its list.  If ELEM is the
   last element in its list, returns the list tail.  Results are
   undefined if ELEM is itself a list tail.
   ELEM이 속한 리스트에서 ELEM 다음 원소를 반환한다. ELEM이 리스트의
   마지막 원소라면 리스트 tail을 반환한다. ELEM 자체가 tail이면 결과는
   정의되지 않는다. */
struct list_elem *
list_next (struct list_elem *elem) {
	ASSERT (is_head (elem) || is_interior (elem));
	return elem->next;
}

/* Returns LIST's tail.

   list_end() is often used in iterating through a list from
   front to back.  See the big comment at the top of list.h for
   an example.
   LIST의 tail을 반환한다.

   list_end()는 리스트를 앞에서 뒤로 순회할 때 자주 사용된다. 예시는
   list.h 상단의 큰 주석을 참고한다. */
struct list_elem *
list_end (struct list *list) {
	ASSERT (list != NULL);
	return &list->tail;
}

/* Returns the LIST's reverse beginning, for iterating through
   LIST in reverse order, from back to front.
   LIST를 뒤에서 앞으로 역순 순회할 때 사용할 역방향 시작 원소를 반환한다. */
struct list_elem *
list_rbegin (struct list *list) {
	ASSERT (list != NULL);
	return list->tail.prev;
}

/* Returns the element before ELEM in its list.  If ELEM is the
   first element in its list, returns the list head.  Results are
   undefined if ELEM is itself a list head.
   ELEM이 속한 리스트에서 ELEM 이전 원소를 반환한다. ELEM이 리스트의
   첫 번째 원소라면 리스트 head를 반환한다. ELEM 자체가 head이면 결과는
   정의되지 않는다. */
struct list_elem *
list_prev (struct list_elem *elem) {
	ASSERT (is_interior (elem) || is_tail (elem));
	return elem->prev;
}

/* Returns LIST's head.

   list_rend() is often used in iterating through a list in
   reverse order, from back to front.  Here's typical usage,
   following the example from the top of list.h:

   for (e = list_rbegin (&foo_list); e != list_rend (&foo_list);
   e = list_prev (e))
   {
   struct foo *f = list_entry (e, struct foo, elem);
   ...do something with f...
   }
   LIST의 head를 반환한다.

   list_rend()는 리스트를 뒤에서 앞으로 역순 순회할 때 자주 사용된다.
   일반적인 사용법은 list.h 상단 예시를 따라 위와 같다.
   */
struct list_elem *
list_rend (struct list *list) {
	ASSERT (list != NULL);
	return &list->head;
}

/* Return's LIST's head.

   list_head() can be used for an alternate style of iterating
   through a list, e.g.:

   e = list_head (&list);
   while ((e = list_next (e)) != list_end (&list))
   {
   ...
   }
   LIST의 head를 반환한다.

   list_head()는 리스트를 순회하는 또 다른 스타일에 사용할 수 있다.
   */
struct list_elem *
list_head (struct list *list) {
	ASSERT (list != NULL);
	return &list->head;
}

/* Return's LIST's tail.
   LIST의 tail을 반환한다. */
struct list_elem *
list_tail (struct list *list) {
	ASSERT (list != NULL);
	return &list->tail;
}

/* Inserts ELEM just before BEFORE, which may be either an
   interior element or a tail.  The latter case is equivalent to
   list_push_back().
   BEFORE 바로 앞에 ELEM을 삽입한다. BEFORE는 내부 원소이거나 tail일 수
   있다. tail 앞에 삽입하는 경우는 list_push_back()과 같다. */
void
list_insert (struct list_elem *before, struct list_elem *elem) {
	ASSERT (is_interior (before) || is_tail (before));
	ASSERT (elem != NULL);

	elem->prev = before->prev;
	elem->next = before;
	before->prev->next = elem;
	before->prev = elem;
}

/* Removes elements FIRST though LAST (exclusive) from their
   current list, then inserts them just before BEFORE, which may
   be either an interior element or a tail.
   현재 리스트에서 FIRST부터 LAST 직전까지의 원소들을 제거한 뒤, BEFORE
   바로 앞에 삽입한다. BEFORE는 내부 원소이거나 tail일 수 있다. */
void
list_splice (struct list_elem *before,
		struct list_elem *first, struct list_elem *last) {
	ASSERT (is_interior (before) || is_tail (before));
	if (first == last)
		return;
	last = list_prev (last);

	ASSERT (is_interior (first));
	ASSERT (is_interior (last));

	/* Cleanly remove FIRST...LAST from its current list.
	   현재 리스트에서 FIRST...LAST 구간을 깔끔하게 제거한다. */
	first->prev->next = last->next;
	last->next->prev = first->prev;

	/* Splice FIRST...LAST into new list.
	   FIRST...LAST 구간을 새 리스트 위치에 이어 붙인다. */
	first->prev = before->prev;
	last->next = before;
	before->prev->next = first;
	before->prev = last;
}

/* Inserts ELEM at the beginning of LIST, so that it becomes the
   front in LIST.
   ELEM을 LIST의 맨 앞에 삽입하여 front가 되게 한다. */
void
list_push_front (struct list *list, struct list_elem *elem) {
	list_insert (list_begin (list), elem);
}

/* Inserts ELEM at the end of LIST, so that it becomes the
   back in LIST.
   ELEM을 LIST의 맨 뒤에 삽입하여 back이 되게 한다. */
void
list_push_back (struct list *list, struct list_elem *elem) {
	list_insert (list_end (list), elem);
}

/* Removes ELEM from its list and returns the element that
   followed it.  Undefined behavior if ELEM is not in a list.

   It's not safe to treat ELEM as an element in a list after
   removing it.  In particular, using list_next() or list_prev()
   on ELEM after removal yields undefined behavior.  This means
   that a naive loop to remove the elements in a list will fail:

 ** DON'T DO THIS **
 for (e = list_begin (&list); e != list_end (&list); e = list_next (e))
 {
 ...do something with e...
 list_remove (e);
 }
 ** DON'T DO THIS **

 Here is one correct way to iterate and remove elements from a
list:

for (e = list_begin (&list); e != list_end (&list); e = list_remove (e))
{
...do something with e...
}

If you need to free() elements of the list then you need to be
more conservative.  Here's an alternate strategy that works
even in that case:

while (!list_empty (&list))
{
struct list_elem *e = list_pop_front (&list);
...do something with e...
}
ELEM을 리스트에서 제거하고, 그 뒤에 있던 원소를 반환한다. ELEM이 리스트에
들어 있지 않으면 동작은 정의되지 않는다.

ELEM을 제거한 뒤에는 ELEM을 리스트 원소처럼 취급하면 안전하지 않다.
특히 제거 후 ELEM에 list_next()나 list_prev()를 사용하면 동작이 정의되지
않는다. 따라서 리스트 원소를 제거하는 순진한 반복문은 실패한다.

위의 "DON'T DO THIS" 예시는 사용하면 안 된다.

리스트를 순회하면서 원소를 제거하는 올바른 방법 중 하나는 위의 두 번째
예시처럼 list_remove()의 반환값으로 다음 반복 위치를 정하는 것이다.

리스트 원소를 free()해야 한다면 더 보수적인 방법이 필요하다. 그 경우에는
마지막 예시처럼 리스트가 빌 때까지 front를 pop하는 전략이 동작한다.
*/
struct list_elem *
list_remove (struct list_elem *elem) {
	ASSERT (is_interior (elem));
	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;
	return elem->next;
}

/* Removes the front element from LIST and returns it.
   Undefined behavior if LIST is empty before removal.
   LIST의 front 원소를 제거하고 반환한다. 제거 전에 LIST가 비어 있으면
   동작은 정의되지 않는다. */
struct list_elem *
list_pop_front (struct list *list) {
	struct list_elem *front = list_front (list);
	list_remove (front);
	return front;
}

/* Removes the back element from LIST and returns it.
   Undefined behavior if LIST is empty before removal.
   LIST의 back 원소를 제거하고 반환한다. 제거 전에 LIST가 비어 있으면
   동작은 정의되지 않는다. */
struct list_elem *
list_pop_back (struct list *list) {
	struct list_elem *back = list_back (list);
	list_remove (back);
	return back;
}

/* Returns the front element in LIST.
   Undefined behavior if LIST is empty.
   LIST의 front 원소를 반환한다. LIST가 비어 있으면 동작은 정의되지 않는다. */
struct list_elem *
list_front (struct list *list) {
	ASSERT (!list_empty (list));
	return list->head.next;
}

/* Returns the back element in LIST.
   Undefined behavior if LIST is empty.
   LIST의 back 원소를 반환한다. LIST가 비어 있으면 동작은 정의되지 않는다. */
struct list_elem *
list_back (struct list *list) {
	ASSERT (!list_empty (list));
	return list->tail.prev;
}

/* Returns the number of elements in LIST.
   Runs in O(n) in the number of elements.
   LIST에 들어 있는 원소 수를 반환한다. 원소 수에 대해 O(n) 시간에 실행된다. */
size_t
list_size (struct list *list) {
	struct list_elem *e;
	size_t cnt = 0;

	for (e = list_begin (list); e != list_end (list); e = list_next (e))
		cnt++;
	return cnt;
}

/* Returns true if LIST is empty, false otherwise.
   LIST가 비어 있으면 true를, 아니면 false를 반환한다. */
bool
list_empty (struct list *list) {
	return list_begin (list) == list_end (list);
}

/* Swaps the `struct list_elem *'s that A and B point to.
   A와 B가 가리키는 `struct list_elem *` 값을 서로 바꾼다. */
static void
swap (struct list_elem **a, struct list_elem **b) {
	struct list_elem *t = *a;
	*a = *b;
	*b = t;
}

/* Reverses the order of LIST.
   LIST의 원소 순서를 뒤집는다. */
void
list_reverse (struct list *list) {
	if (!list_empty (list)) {
		struct list_elem *e;

		for (e = list_begin (list); e != list_end (list); e = e->prev)
			swap (&e->prev, &e->next);
		swap (&list->head.next, &list->tail.prev);
		swap (&list->head.next->prev, &list->tail.prev->next);
	}
}

/* Returns true only if the list elements A through B (exclusive)
   are in order according to LESS given auxiliary data AUX.
   A부터 B 직전까지의 리스트 원소들이 보조 데이터 AUX와 비교 함수 LESS에
   따라 정렬되어 있을 때만 true를 반환한다. */
static bool
is_sorted (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) {
	if (a != b)
		while ((a = list_next (a)) != b)
			if (less (a, list_prev (a), aux))
				return false;
	return true;
}

/* Finds a run, starting at A and ending not after B, of list
   elements that are in nondecreasing order according to LESS
   given auxiliary data AUX.  Returns the (exclusive) end of the
   run.
   A through B (exclusive) must form a non-empty range.
   A에서 시작하고 B를 넘지 않는, LESS와 보조 데이터 AUX 기준으로
   비내림차순인 리스트 원소들의 run을 찾는다. 반환값은 run의 끝(제외)이다.
   A부터 B 직전까지는 비어 있지 않은 범위여야 한다. */
static struct list_elem *
find_end_of_run (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) {
	ASSERT (a != NULL);
	ASSERT (b != NULL);
	ASSERT (less != NULL);
	ASSERT (a != b);

	do {
		a = list_next (a);
	} while (a != b && !less (a, list_prev (a), aux));
	return a;
}

/* Merges A0 through A1B0 (exclusive) with A1B0 through B1
   (exclusive) to form a combined range also ending at B1
   (exclusive).  Both input ranges must be nonempty and sorted in
   nondecreasing order according to LESS given auxiliary data
   AUX.  The output range will be sorted the same way.
   A0부터 A1B0 직전까지의 구간과 A1B0부터 B1 직전까지의 구간을 병합하여,
   B1 직전에서 끝나는 하나의 결합된 구간을 만든다. 두 입력 구간은 모두
   비어 있지 않아야 하고, LESS와 보조 데이터 AUX 기준으로 비내림차순
   정렬되어 있어야 한다. 출력 구간도 같은 방식으로 정렬된다. */
static void
inplace_merge (struct list_elem *a0, struct list_elem *a1b0,
		struct list_elem *b1,
		list_less_func *less, void *aux) {
	ASSERT (a0 != NULL);
	ASSERT (a1b0 != NULL);
	ASSERT (b1 != NULL);
	ASSERT (less != NULL);
	ASSERT (is_sorted (a0, a1b0, less, aux));
	ASSERT (is_sorted (a1b0, b1, less, aux));

	while (a0 != a1b0 && a1b0 != b1)
		if (!less (a1b0, a0, aux))
			a0 = list_next (a0);
		else {
			a1b0 = list_next (a1b0);
			list_splice (a0, list_prev (a1b0), a1b0);
		}
}

/* Sorts LIST according to LESS given auxiliary data AUX, using a
   natural iterative merge sort that runs in O(n lg n) time and
   O(1) space in the number of elements in LIST.
   보조 데이터 AUX와 비교 함수 LESS에 따라 LIST를 정렬한다. 자연 반복
   병합 정렬을 사용하며, LIST의 원소 수에 대해 O(n lg n) 시간과 O(1)
   공간으로 실행된다. */
void
list_sort (struct list *list, list_less_func *less, void *aux) {
	size_t output_run_cnt;        /* Number of runs output in current pass.
	                                 현재 pass에서 출력된 run의 수. */

	ASSERT (list != NULL);
	ASSERT (less != NULL);

	/* Pass over the list repeatedly, merging adjacent runs of
	   nondecreasing elements, until only one run is left.
	   하나의 run만 남을 때까지 리스트를 반복해서 훑으며 인접한 비내림차순
	   run들을 병합한다. */
	do {
		struct list_elem *a0;     /* Start of first run.
		                             첫 번째 run의 시작. */
		struct list_elem *a1b0;   /* End of first run, start of second.
		                             첫 번째 run의 끝이자 두 번째 run의 시작. */
		struct list_elem *b1;     /* End of second run.
		                             두 번째 run의 끝. */

		output_run_cnt = 0;
		for (a0 = list_begin (list); a0 != list_end (list); a0 = b1) {
			/* Each iteration produces one output run.
			   각 반복은 하나의 출력 run을 만든다. */
			output_run_cnt++;

			/* Locate two adjacent runs of nondecreasing elements
			   A0...A1B0 and A1B0...B1.
			   A0...A1B0와 A1B0...B1이라는 두 인접한 비내림차순 run을 찾는다. */
			a1b0 = find_end_of_run (a0, list_end (list), less, aux);
			if (a1b0 == list_end (list))
				break;
			b1 = find_end_of_run (a1b0, list_end (list), less, aux);

			/* Merge the runs.
			   run들을 병합한다. */
			inplace_merge (a0, a1b0, b1, less, aux);
		}
	}
	while (output_run_cnt > 1);

	ASSERT (is_sorted (list_begin (list), list_end (list), less, aux));
}

/* Inserts ELEM in the proper position in LIST, which must be
   sorted according to LESS given auxiliary data AUX.
   Runs in O(n) average case in the number of elements in LIST.
   LESS와 보조 데이터 AUX 기준으로 정렬되어 있어야 하는 LIST의 적절한
   위치에 ELEM을 삽입한다. LIST의 원소 수에 대해 평균 O(n) 시간에 실행된다. */
void
list_insert_ordered (struct list *list, struct list_elem *elem,
		list_less_func *less, void *aux) {
	struct list_elem *e;

	ASSERT (list != NULL);
	ASSERT (elem != NULL);
	ASSERT (less != NULL);

	for (e = list_begin (list); e != list_end (list); e = list_next (e))
		if (less (elem, e, aux))
			break;
	return list_insert (e, elem);
}

/* Iterates through LIST and removes all but the first in each
   set of adjacent elements that are equal according to LESS
   given auxiliary data AUX.  If DUPLICATES is non-null, then the
   elements from LIST are appended to DUPLICATES.
   LIST를 순회하면서, LESS와 보조 데이터 AUX 기준으로 같은 인접 원소들의
   집합에서 첫 번째 원소만 남기고 나머지를 제거한다. DUPLICATES가 null이
   아니면 LIST에서 제거된 원소들을 DUPLICATES에 덧붙인다. */
void
list_unique (struct list *list, struct list *duplicates,
		list_less_func *less, void *aux) {
	struct list_elem *elem, *next;

	ASSERT (list != NULL);
	ASSERT (less != NULL);
	if (list_empty (list))
		return;

	elem = list_begin (list);
	while ((next = list_next (elem)) != list_end (list))
		if (!less (elem, next, aux) && !less (next, elem, aux)) {
			list_remove (next);
			if (duplicates != NULL)
				list_push_back (duplicates, next);
		} else
			elem = next;
}

/* Returns the element in LIST with the largest value according
   to LESS given auxiliary data AUX.  If there is more than one
   maximum, returns the one that appears earlier in the list.  If
   the list is empty, returns its tail.
   LESS와 보조 데이터 AUX 기준으로 LIST에서 가장 큰 값을 가진 원소를
   반환한다. 최댓값이 여러 개라면 리스트에서 더 앞에 나타나는 원소를
   반환한다. 리스트가 비어 있으면 tail을 반환한다. */
struct list_elem *
list_max (struct list *list, list_less_func *less, void *aux) {
	struct list_elem *max = list_begin (list);
	if (max != list_end (list)) {
		struct list_elem *e;

		for (e = list_next (max); e != list_end (list); e = list_next (e))
			if (less (max, e, aux))
				max = e;
	}
	return max;
}

/* Returns the element in LIST with the smallest value according
   to LESS given auxiliary data AUX.  If there is more than one
   minimum, returns the one that appears earlier in the list.  If
   the list is empty, returns its tail.
   LESS와 보조 데이터 AUX 기준으로 LIST에서 가장 작은 값을 가진 원소를
   반환한다. 최솟값이 여러 개라면 리스트에서 더 앞에 나타나는 원소를
   반환한다. 리스트가 비어 있으면 tail을 반환한다. */
struct list_elem *
list_min (struct list *list, list_less_func *less, void *aux) {
	struct list_elem *min = list_begin (list);
	if (min != list_end (list)) {
		struct list_elem *e;

		for (e = list_next (min); e != list_end (list); e = list_next (e))
			if (less (e, min, aux))
				min = e;
	}
	return min;
}
