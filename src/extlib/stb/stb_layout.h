// stb_layout - v0.01 - public domain - http://nothings.org/stb/stb_layout.h
// by Sean Barrett, 2009-2010
//
// This file implements the logic for a box layout model. It is designed to
// modular from the details of the things that it is laying out; you run
// through your content and build an appropriate hierarchy of stbl_box_tes,
// and then invoke the layout engine to get the layout for the boxes, and
// then copy out the results and free it.
//
// My hope is that people will contribute fixes and improvements to the
// layout model, and we can all share them. This may only really viable if
// the model can remain fairly simple. (Technically the model could get crazy
// as long as the API is simple; I'm not sure I'd be thrilled with a really
// complex implementation though, since then it'll be hard for people to
// cooperate on fixing.)
//
// It does not do tables but that might be a worthwhile improvement.
//
// MODEL
//
// The basic box model here is very simple: you have a collection of
// "horizontal" and "vertical" layout boxes. There are no tables; you can't
// align elements in one box with elements in another unrelated box.
//
// Each box contains 0 or more other boxes, all laid out horizontally or
// vertically. Each box (even if it contains other boxes) may specify a
// minimum size requirement (in pixels, or relative to the parent). Boxes
// will then expand to fit their contents *and* the minimums.
//
// Containers force their children to be uniform: e.g. a horizontal
// container is not just as wide as the sum of its children, but it
// also forces all of its children to be the same height. (Optionally,
// it can force all children to be the same width as well.)
//
// BOXES
//
// Each box has the following properties:
//     - is it horizontal or vertical
//     - its requested width/height propreties
//     - any child boxes
//     - the laid-out width/height computed by the algorithm
//
// Width & height are specified by giving a type, and then two floats.
// The possible types are:
//
//    STBL_BM_exact    - the box is forced to a pixel size equal to the w/h properties
//    STBL_BM_minpix   - the box must be at least as large as the w/h properties
//    STBL_BM_percent  - the box is expressed as a fraction of the parent
//    STBL_BM_padding  - the w/h properties are extra pixels to pad the contents by
//
//

enum
{
	STBL_BM_minpix,       // b->size is the total pixels
	STBL_BM_padding,      // b->size is the pixels of padding
	STBL_BM_percent,      // b->size is the fraction of the parent
	STBL_BM_exact,        // b->size is the total pixels, and it can't expand
};

#ifdef STB_LAYOUT_IMPLEMENTATION

typedef struct stbl_box_t stbl_box_t;

struct stbl_box_t
{
	stbl_box_t *children;            // pointer to first child box
	stbl_box_t *next;                // pointer to first sibling box
	float w,h;                      // current proposed size in pixels
	float min_w,min_h;              // initial minimum required size in pixels, computed only once
	float req_w,req_h;              // requested size in pixels/percent of parent
	unsigned char info_type;        // which type of requested size
	unsigned char vertical:1;       // true if children laid out vertically
	unsigned char changed_by_p:1;   // true if w/h was changed by parent
	unsigned char match_widths:1;   // true if children should all have same width
	unsigned char match_heights:1;  // true if children should all have same height
	unsigned char can_shrink:1;     // true if box can be smaller than minimum if top-level height is forced
};  // 36b

#ifndef STBL_ALLOC_BOX
#include <stdlib.h>
#define STBL_ALLOC_BOX(s) malloc(s)
#define STBL_FREE_BOX(p) free(p)
#endif

#ifndef STBL_EPSILON
#define STBL_EPSILON (0.5f)   // half a pixel
#endif

#ifndef STBL_ASSERT
#include <assert.h>
#define STBL_ASSERT(x) assert(x)
#endif

// Implementation:
//
// pass one:
//    copy min_w/min_h from the requirements
//
// pass two:
//    compute size of parent from children; compute size of children from parent
//    traverse up tree; repeat until none are 'changed'

static void stbl_initial_value(stbl_box_t *p, stbl_box_t *b)
{
	if (b)
	{
		switch (b->info_type)
		{
		case STBL_BM_padding:
			b->min_w = b->req_w;
			b->min_h = b->req_h;
			break;
		case STBL_BM_exact:
		case STBL_BM_minpix:
			b->min_w = b->req_w;
			b->min_h = b->req_h;
			break;
		case STBL_BM_percent:
			b->min_w = b->req_w * p->min_w;
			b->min_h = b->req_h * p->min_h;
			break;
		default:
			STBL_ASSERT(0);
			break;
		}
		b->w = b->min_w;
		b->h = b->min_h;
		b->changed_by_p = 1;
		stbl_initial_value(b, b->children);
		stbl_initial_value(p, b->next);
	}
}

#define STBL_EPSILON_EQUAL(x,y) \
    ((x)-(y) <= STBL_EPSILON && (x)-(y) >= -STBL_EPSILON)

// we traverse the tree upwards and allow updated children
// to affect a node and an updated node to affect the
// children. if any children are affected, they set a flag
// and are updated on the next pass over the tree. changes
// to the node itself are propogated to its parent in this
// pass (it's a postorder traversal)
static int stbl_update_from_children(stbl_box_t *b, int *updated)
{
	// compute the size of a box based on its children
	int n=0, changed=0;

	float fw=0,fh=0,w=0,h=0;
	float max_w=0,max_h=0;
	float nw=0,nh=0;
	stbl_box_t *z = b->children;

	// iterate over all children and add up their sizes
	//   add up pixel counts separately from percentages
	while (z)
	{
		if (z->w > max_w) max_w = z->w;
		if (z->h > max_h) max_h = z->h;

		if (z->info_type == STBL_BM_percent)
		{
			if (b->vertical)
			{
				fh += z->req_h;
				if (nh < z->h/z->req_h) nh = z->h / z->req_h;
			}
			else
			{
				fw += z->req_w;
				if (nw < z->w/z->req_w) nw = z->w / z->req_w;
			}
		}
		else
		{
			w += z->w;
			h += z->h;
		}
		z = z->next;
		++n;
	}

	STBL_ASSERT(n != 0);

	// now compute the size that satisfies the constraints;
	// store into w,h
	if (b->vertical)
	{
		w = max_w > b->min_w ? max_w : b->min_w;
		// if
		if (b->info_type == STBL_BM_padding)
		{
			w += b->req_w;
			h += b->req_h;
		}
		if (b->match_heights)
			h = max_h * n; // all things must be same height!
		// @TODO: only apply this to things that aren't percentages
		else if (fh >= 1)
			h = 0;
		else
		{
			// require:  H * fh + h + padding == H
			//    H * (1-fh) == h
			if (fh)
				h = h / (1-fh);
		}
		if (h < nh)        h = nh;
		if (h < b->min_h)  h = b->min_h;
	}
	else
	{
		h = max_h > b->min_h ? max_h : b->min_h;
		if (b->info_type == STBL_BM_padding)
		{
			w += b->req_w;
			h += b->req_h;
		}
		if (b->match_widths)
			w = max_w * n;
		else if (fw >= 1)
			w = 0;
		else
		{
			// require:  H * fh + h + padding == H
			//    H * (1-fh) == h
			if (fw)
				w = w / (1-fw);
		}
		if (w < nw)        w = nw;
		if (w < b->min_w)  w = b->min_w;
	}

	// now we have new w/h values, so store them
	if (!STBL_EPSILON_EQUAL(w,b->w) || !STBL_EPSILON_EQUAL(h, b->h))
	{
		b->w = w;
		b->h = h;
		changed = 1;
	}
	else
		changed = 0;

	// now propogate the new size to the children
	z = b->children;

	if (z)
	{
		// compute unpadded w/h
		float ew = b->w, eh = b->h;
		if (b->info_type == STBL_BM_padding)
		{
			ew -= b->req_w;
			eh -= b->req_h;
		}

		while (z)
		{
			if (b->vertical)
			{
				w = ew * (b->match_widths ? 1 : z->req_w);
				if (b->match_heights)
					h = eh / n;
				else if (z->info_type == STBL_BM_percent)
					h = eh * z->req_h;
				else
					h = z->h;
			}
			else
			{
				h = eh * (b->match_heights ? 1 : z->req_h);
				if (b->match_widths)
					w = ew / n;
				else if (z->info_type == STBL_BM_percent)
					w = ew * z->req_w;
				else
					w = z->w;
			}

			if (!STBL_EPSILON_EQUAL(w,z->w) || !STBL_EPSILON_EQUAL(h, z->h))
			{
				z->w = w;
				z->h = h;
				z->changed_by_p = 1;
				*updated = 1;
			}
			z = z->next;
		}
	}

	return changed;
}

// returns true if this box changes size
static int stbl_propagate_constraints(stbl_box_t *b, int *updated)
{
	int any_changed = 0;
	stbl_box_t *z = b->children;
	while (z)
	{
		if (stbl_propagate_constraints(z, updated))
			any_changed = 1;
		z = z->next;
	}
	if (any_changed || (b->changed_by_p && b->children))
		return stbl_update_from_children(b, updated);
	else
		return 0;
}

void stbl_layout(stbl_box_t *b, float w, float h, int max_passes)
{
	int i;
	stbl_box_t root;
	root.min_w = w;
	root.min_h = h;
	stbl_initial_value(&root, b);
	for (i=0; i < max_passes; ++i)
	{
		int unpropagated_update=0;
		stbl_propagate_constraints(b, &unpropagated_update);
		if (!unpropagated_update)
			break;
	}
}

void *stbl_box(int vertical, int type, float w, float h, int mw, int mh, int cs)
{
	stbl_box_t *b = (stbl_box_t *) STBL_ALLOC_BOX(sizeof(*b));
	b->vertical = vertical;
	b->info_type = type;
	b->req_w = w;
	b->req_h = h;
	b->children = NULL;
	b->next = NULL;
	b->match_heights = mh;
	b->match_widths = mw;
	b->can_shrink = cs;
	return b;
}

void *stbl_hbox(int type, float w, float h)
{
	return stbl_box(0, type, w, h, 0, 1, 0);
}

void *stbl_vbox(int type, float w, float h)
{
	return stbl_box(1, type, w, h, 1, 0, 0);
}

void *stbl_add(stbl_box_t *parent, stbl_box_t *child)
{
	STBL_ASSERT(child->next == NULL);
	child->next = parent->children;
	parent->children = child;
	return parent;
}

#endif  // STB_LAYOUT_IMPLEMENTATIOn

#ifdef STB_LAYOUT_TEST
// construct a sample box hierarchy and test it
//
//   box padding 4,4
//   .vbox
//   ..hbox
//   ...pixels 40,20
//   ...pixels 50,20
//   ...percent 50,100
//   ..hbox
//   ...percent 50,100
//   ....hbox padding 10,10
//   .....percent 20,100
//   .....pixels 50,50
#include <stdio.h>

static void dump_tree(void *t, int depth)
{
	int i;
	stbl_box_t *b = (stbl_box_t *) t;
	for (i=0; i < depth; ++i) putchar(' ');
	printf("%sbox: %5.2f %5.2f\n", b->vertical ? "v" : "h", b->w, b->h);

	b = b->children;
	while (b)
	{
		dump_tree(b, depth+1);
		b = b->next;
	}
}

int main(int argc, char **argv)
{
	int i;

	void *tree = stbl_hbox(STBL_BM_padding, 4,4);
	void *p, *q;
	void *b0 = stbl_hbox(0,0,0);
	void *b1 = stbl_hbox(0,0,0);

#if 1
	// make top 3 levels of tree
	stbl_add(tree, stbl_add(stbl_add(stbl_vbox(0,0,0), b0), b1));

	// fill out details of b0
	stbl_add(stbl_add(stbl_add(b0, stbl_hbox(STBL_BM_minpix, 40, 20)),
	                  stbl_hbox(STBL_BM_minpix, 50, 25)),
	         stbl_hbox(STBL_BM_percent,0.50,1.00));

	// fill out the details of b1
	p = stbl_hbox(STBL_BM_percent, 40, 100);
	stbl_add(b1, p);
	q = stbl_hbox(STBL_BM_padding, 10, 10);
	stbl_add(p, q);

	stbl_add(stbl_add(q, stbl_hbox(STBL_BM_percent, 0.20f,1.00)),
	         stbl_hbox(STBL_BM_minpix, 50,50));
#else
	stbl_add(stbl_add(tree, stbl_hbox(STBL_BM_percent, 0.20f,1.00)),
	         stbl_hbox(STBL_BM_minpix, 50,50));
#endif
	for (i=1; i < 10; ++i)
	{
		stbl_layout(tree, 100,100, i);
		dump_tree(tree, 0);
	}

	return 0;
}
#endif