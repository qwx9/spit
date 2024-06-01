#include "a.h"
#include "style.h"

Mousectl	*mc;
Keyboardctl *kc;
Image		*cols[Ncols];
char		*ftitlename;
Sfont		*ftitle;
char		*ftextname;
Sfont		*ftext;
char		*ffixedname;
Sfont		*ffixed;
Sdopts		 opts = {.prefilter = 0, .gamma = 1.0 };
int			fullscreen;
Rectangle	screenr;
Image		*slides[Maxslides];
int			nslides;
int			curslide;
Image		*bol;
Image		*bullet;
int			nodraw;

void
ladd(Lines *l, char *s)
{
	if(l->nlines == Maxlines)
		sysfatal("too many lines");
	l->lines[l->nlines++] = strdup(s);
}

void
lclear(Lines *l)
{
	int i;

	for(i = 0; i < l->nlines; i++)
		free(l->lines[i]);
	l->nlines = 0;
}

void
error(char *f, int l, char *m)
{
	fprint(2, "error: %s at %s:%d\n", m, f, l);
	threadexitsall("error");
}

Image*
addslide(void)
{
	++nslides;
	if(nslides >= Maxslides)
		sysfatal("too many slides");
	slides[nslides] = allocimage(display, display->image->r, screen->chan, 0, coldefs[Cbg]);
	if(slides[nslides] == nil)
		sysfatal("allocimage: %r");
	return slides[nslides];
}

Point
rendertitle(Image *b, Point p, char *s)
{
	Rectangle r;
	Image *i;

	r = pt_textrect(ftitle, s);
	i = pt_textdraw(ftitle, s, r, &opts);
	draw(b, rectaddpt(r, p), cols[Cfg], i, ZP);
	freeimage(i);
	p.y += Dy(r);
	line(b, Pt(p.x, p.y), Pt(b->r.max.x - margin, p.y), 0, 0, 2, cols[Cfg], ZP);
	p.y += Dy(r);
	return p;
}

Point
rendertext(Image *b, Point p, char *s)
{
	Rectangle r;
	Image *i;

	r = pt_textrect(ftext, s);
	if(strlen(s) > 0){
		i = pt_textdraw(ftext, s, r, &opts);
		draw(b, Rect(p.x, p.y, p.x+Dx(bol->r), p.y+Dy(bol->r)), bol, 0, ZP);
		draw(b, rectaddpt(r, Pt(p.x + Dx(bol->r) + padding, p.y)), cols[Cfg], i, ZP);
		freeimage(i);
	}
	p.y += Dy(r)*lineheight;
	return p;
}

Point
renderlist(Image *b, Point p, Lines *lines)
{
	Point q;
	Rectangle r;
	Image *t;
	int i;

	p.x += Dx(bol->r);
	for(i = 0; i < lines->nlines; i++){
		draw(b, rectaddpt(bullet->r, p), bullet, nil, ZP);
		q = addpt(p, Pt(Dx(bullet->r) + padding, 0));
		r = pt_textrect(ftext, lines->lines[i]);
		t = pt_textdraw(ftext, lines->lines[i], r, &opts);
		draw(b, rectaddpt(r, q), cols[Cfg], t, ZP);
		freeimage(t);
		p.y += Dy(r);
	}
	p.x -= Dx(bol->r);
	return p;
}

Point
renderquote(Image *b, Point p, Lines *lines)
{
	Rectangle r[Maxlines], br;
	Image *t;
	int i, maxw, maxh;

	maxw = 0;
	maxh = 0;
	for(i = 0; i < lines->nlines; i++){
		r[i] = pt_textrect(ftext, lines->lines[i]);
		maxh += Dy(r[i]);
		if(Dx(r[i]) > maxw)
			maxw = Dx(r[i]);
	}
	p.x += Dx(bol->r) + margin;
	br = Rect(p.x, p.y, p.x + 1.5*maxw + 2*padding, p.y + maxh + 2*padding);
	draw(b, br, cols[Cqbg], nil, ZP);
	line(b, br.min, Pt(br.min.x, br.max.y), 0, 0, padding/2, cols[Cqbord], ZP);
	p.x += padding;
	p.y += padding;
	for(i = 0; i < lines->nlines; i++){
		t = pt_textdraw(ftext, lines->lines[i], r[i], &opts);
		draw(b, rectaddpt(r[i], Pt(p.x+padding, p.y)), cols[Cfg], t, ZP);
		freeimage(t);
		p.y += Dy(r[i]);
	}
	p.x -= padding;
	p.x -= Dx(bol->r) + margin;
	return p;
}


Point
rendercode(Image *b, Point p, Lines *lines)
{
	Rectangle r[Maxlines], br;
	Image *t;
	int i, maxw, maxh;

	maxw = 0;
	maxh = 0;
	for(i = 0; i < lines->nlines; i++){
		r[i] = pt_textrect(ffixed, lines->lines[i]);
		maxh += Dy(r[i]);
		if(Dx(r[i]) > maxw)
			maxw = Dx(r[i]);
	}
	p.x += Dx(bol->r) + margin;
	br = Rect(p.x, p.y, p.x + 1.5*maxw + 2*padding, p.y + maxh + 2*padding);
	draw(b, br, cols[Ccbg], nil, ZP);
	border(b, br, 2, cols[Ccbord], ZP);
	p.y += padding;
	for(i = 0; i < lines->nlines; i++){
		t = pt_textdraw(ffixed, lines->lines[i], r[i], &opts);
		draw(b, rectaddpt(r[i], Pt(p.x+padding, p.y)), cols[Cfg], t, ZP);
		freeimage(t);
		p.y += Dy(r[i]);
	}
	p.x -= Dx(bol->r) + margin;
	return p;
}

Point
renderimage(Image *b, Point p, char *f)
{
	Image *i;
	int fd;

	fd = open(f, OREAD);
	if(fd <= 0)
		sysfatal("open: %r");
	i = readimage(display, fd, 0);
	draw(b, rectaddpt(i->r, p), i, nil, ZP);
	p.y += Dy(i->r) + margin;
	freeimage(i);
	close(fd);
	return p;
}

char*
skipws(char *s)
{
	while(*s == ' ' || *s == '\t')
		++s;
	return s;
}

ulong
estrtoul(char *f, int line, char *s)
{
	char *e;
	ulong c;

	c = strtoul(s, &e, 16);
	if(e == s || e == nil)
		error(f, line, "invalid number");
	return (c << 8) | 0xff;
}	

void
parsestyle(char *f, int line, char *s)
{
	char k[32] = {0}, *p;

	s += 6; /* skip '@style' */
	if(s[0] != '[')
		error(f, line, "expected '[' character in style declaration");
	p = strchr(s, ']');
	if(p == nil)
		error(f, line, "expected ']' character in style declaration");
	if(p == s+1)
		error(f, line, "empty style declaration");
	if(p-s >= 32)
		error(f, line, "invalid key in style declaration");
	strncpy(k, s+1, p-s-1);
	s = skipws(p+1);
	if(*s != '=')
		error(f, line, "expected '=' character in style declaration");
	s = skipws(s+1);
	if(*s == 0)
		error(f, line, "empty style value");
	if(strcmp(k, "margin") == 0){
		margin = atoi(s);
		if(margin == 0) error(f, line, "invalid 'margin' value");
	}else if(strcmp(k, "padding") == 0){
		padding = atoi(s);
		if(padding == 0) error(f, line, "invalid 'padding' value");
	}else if(strcmp(k, "lineheight") == 0){
		lineheight = atof(s);
		if(lineheight < 1.0) error(f, line, "invalid 'lineheight' value");
	}else if(strcmp(k, "color.bg") == 0)
		coldefs[Cbg] = estrtoul(f, line, s);
	else if(strcmp(k, "color.fg") == 0)
		coldefs[Cfg] = estrtoul(f, line, s);
	else if(strcmp(k, "color.quotebg") == 0)
		coldefs[Cqbg] = estrtoul(f, line, s);
	else if(strcmp(k, "color.quoteborder") == 0)
		coldefs[Cqbord] = estrtoul(f, line, s);
	else if(strcmp(k, "color.codebg") == 0)
		coldefs[Ccbg] = estrtoul(f, line, s);
	else if(strcmp(k, "color.codeborder") == 0)
		coldefs[Ccbord] = estrtoul(f, line, s);
	else if(strcmp(k, "title.font") == 0)
		ftitlename = strdup(s);
	else if(strcmp(k, "title.size") == 0)
		ftitlesz = atof(s);
	else if(strcmp(k, "text.font") == 0)
		ftextname = strdup(s);
	else if(strcmp(k, "text.size") == 0)
		ftextsz = atof(s);
	else if(strcmp(k, "fixed.font") == 0)
		ffixedname = strdup(s);
	else if(strcmp(k, "fixed.size") == 0)
		ffixedsz = atof(s);
	else
		error(f, line, "unknown style key");		
}

void
initimages(void)
{
	Point p[4];

	bol = allocimage(display, Rect(0, 0, ftextsz, ftextsz), screen->chan, 0, coldefs[Cbg]);
	p[0] = Pt(0.25*ftextsz, 0.25*ftextsz);
	p[1] = Pt(0.25*ftextsz, 0.75*Dy(bol->r));
	p[2] = Pt(0.75*ftextsz, 0.50*Dy(bol->r));
	p[3] = p[0];
	fillpoly(bol, p, 4, 0, cols[Cfg], ZP);
	bullet = allocimage(display, Rect(0, 0, ftextsz, ftextsz), screen->chan, 0, coldefs[Cbg]);
	fillellipse(bullet, Pt(0.5*ftextsz, 0.5*ftextsz), 0.15*ftextsz, 0.15*ftextsz, cols[Cfg], ZP);
}

void
loadstyle(char *f)
{
	int i;

	if(ftitlename == nil){
		fprint(2, "%s: no title font defined", f);
		threadexitsall("missing font");
	}
	for(i = 0; i < Ncols; i++)
		cols[i] = ealloccol(coldefs[i]);
	ftitle = loadsfont(ftitlename, ftitlesz);
	ftext  = loadsfont(ftextname ? ftextname : ftitlename, ftextsz);
	ffixed = loadsfont(ffixedname ? ffixedname : ftitlename, ffixedsz);
	initimages();
}

void
render(char *f)
{
	enum { Sstart, Scomment, Scontent, Slist, Squote, Scode };
	Biobuf *bp;
	char *l;
	int s, ln;
	Image *b;
	Point p;
	Lines lines = {0};

	s = Sstart;
	b = nil;
	ln = 0;
	nslides = -1;
	curslide = 0;
	if((bp = Bopen(f, OREAD)) == nil)
		sysfatal("Bopen: %r");
	for(;;){
		l = Brdstr(bp, '\n', 1);
		++ln;
		if(l == nil)
			break;
Again:
		switch(s){
		case Sstart:
			if(l[0] == ';' || l[0] == 0){
				free(l);
				continue;
			}
			if(strncmp(l, "@style", 6) == 0){
				parsestyle(f, ln, l);
				free(l);
				continue;
			}
			if(l[0] != '#') error(f, ln, "expected title line");
			if(nslides == -1) /* all style parsed but not slide rendered yet */
				loadstyle(f);
Title:
			p = Pt(margin, margin);
			b = addslide();
			p = rendertitle(b, p, l+2);
			s = Scontent;
			break;
		case Scomment:
			s = Scontent;
			break;
		case Scontent:
			if(l[0] == '#')
				goto Title;
			else if(l[0] == '-'){
				s = Slist;
				goto Again;
			}else if(l[0] == '>'){
				s = Squote;
				goto Again;
			}else if(strncmp(l, "```", 3) == 0){
				s = Scode;
				break;
			}else if(l[0] == '!')
				p = renderimage(b, p, l+2);
			else if(l[0] == ';'){
				s = Scomment;
				break;
			}else
				p = rendertext(b, p, l);
			break;
		case Slist:
			if(l[0] != '-'){
				p = renderlist(b, p, &lines);
				lclear(&lines);
				s = Scontent;
				goto Again;
			}
			ladd(&lines, l+2);
			break;
		case Squote:
			if(l[0] != '>'){
				p = renderquote(b, p, &lines);
				lclear(&lines);
				s = Scontent;
				goto Again;
			}
			ladd(&lines, l+2);
			break;
		case Scode:
			if(strncmp(l, "```", 3) == 0){
				p = rendercode(b, p, &lines);
				lclear(&lines);
				s = Scontent;
				break;
			}
			ladd(&lines, l);
			break;
		}
		free(l);
	}
	if(nslides == -1)
		error(f, ln, "no slides parsed");
}

void
barf(void)
{
	int fd;
	char path[64];
	Image **i;

	for(i=slides; i<slides+nslides+1; i++){
		snprint(path, sizeof path, "spit.%03zd.bit", i - slides);
		if((fd = create(path, OWRITE, 0644)) < 0)
			sysfatal("open: %r");
		writeimage(fd, *i, 0);
		close(fd);
	}
}

void
redraw(void)
{
	draw(screen, screen->r, slides[curslide], nil, ZP);
	flushimage(display, 1);
}

void
wresize(int x, int y, int w, int h)
{
	int fd, n;
	char buf[255];

	fd = open("/dev/wctl", OWRITE|OCEXEC);
	if(fd < 0)
		sysfatal("open: %r");
	n = snprint(buf, sizeof buf, "resize -r %d %d %d %d", x, y, w, h);
	if(write(fd, buf, n) != n)
		fprint(2, "write error: %r\n");
	close(fd);
}

void
resize(void)
{
	if(fullscreen)
		wresize(0, 0, 9999, 9999);
	redraw();
}

void
togglefullscreen(void)
{
	int x, y, w, h;

	if(fullscreen){
		fullscreen = 0;
		x = screenr.min.x;
		y = screenr.min.y;
		w = screenr.max.x;
		h = screenr.max.y;
	}else{
		fullscreen = 1;
		x = 0;
		y = 0;
		w = 9999;
		h = 9999;
	}
	wresize(x, y, w, h);
	redraw();
}

void
usage(void)
{
	fprint(2, "%s [-n] <filename>\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char **argv)
{
	enum { Emouse, Eresize, Ekeyboard };
	char *f;
	Mouse m;
	Rune k;
	Alt alts[] = {
		{ nil, &m,  CHANRCV },
		{ nil, nil, CHANRCV },
		{ nil, &k,  CHANRCV },
		{ nil, nil, CHANEND },
	};

	ftitlename = nil;
	ftextname = nil;
	ffixedname = nil;
	ARGBEGIN{
	case 'n':
		nodraw = 1;
		break;
	default:
		usage();
	}ARGEND;
	if((f = *argv) == nil){
		fprint(2, "missing filename\n");
		usage();
	}
	setfcr(getfcr() & ~(FPZDIV | FPOVFL | FPINVAL));
	if(initdraw(nil, nil, argv0) < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	display->locking = 0;
	alts[Emouse].c = mc->c;
	alts[Eresize].c = mc->resizec;
	alts[Ekeyboard].c = kc->c;
	memimageinit();
	fullscreen = 0;
	screenr = screen->r;
	render(f);
	if(nodraw){
		barf();
		threadexitsall(nil);
	}
	resize();
	for(;;){
		switch(alt(alts)){
		case Emouse:
			break;
		case Eresize:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			resize();
			break;
		case Ekeyboard:
			switch(k){
			case Kdel:
			case 'q':
				if(fullscreen)
					togglefullscreen();
				threadexitsall(nil);
				break;
			case 'f':
				togglefullscreen();
				break;
			case Kbs:
			case Kleft:
				if(curslide > 0){
					curslide--;
					redraw();
				}
				break;
			case ' ':
			case Kright:
				if(curslide < nslides){
					curslide++;
					redraw();
				}
				break;
			case Khome:
				curslide = 0;
				redraw();
				break;
			case Kend:
				curslide = nslides;
				redraw();
				break;
			}
			break;
		}
	}
}
