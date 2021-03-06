/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
// static struct disk *swap_disk; // 수정하지 말랬지만, 그냥 파일 분리했음 - Jack
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swapdisk_init();
}

/* eleshock */
/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	if (page == NULL || kva == NULL) return false;
	struct uninit_page *uninit = &page->uninit;
	memset(uninit, 0, sizeof(struct uninit_page));

	/* Set up the handler */
	page->operations = &anon_ops; 

	struct anon_page *anon_page = &page->anon;
	anon_page->sub_type = VM_SUBTYPE(type);
	anon_page->swap_slot = -1;
	return true;
}

/* Jack */
/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	bool ret = swapdisk_swap_in(anon_page->swap_slot, kva, false);
	if (ret == true)
		anon_page->swap_slot = -1;
	return ret;
}

/* Jack */
/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if ((anon_page->swap_slot = swapdisk_swap_out(page->frame->kva)) == -1)
		return false;
	return true;
}

/* eleshock */
/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	/* eleshock */
	struct frame *fr = page->frame;
	/* Jack */
	if (fr != NULL)
	{
		ft_delete(fr);
		// palloc_free_page(fr->kva); // pml4 destroy에서 알아서 해줌
		free(fr);
	} else {
		swapdisk_free_swap_slot(anon_page->swap_slot);
	}
}
