#include "a.h"
#include "style.h"

Mousectl	*mc;
Keyboardctl *kc;
Image		*cols[Ncols];
char		*ftitlename;
Sfont		*ftitle;
char		*ftextname;
Sfont		*ftext;
Sfont		*frtext;
Sfont		*fnum;
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
		sysfatal("slide %d: too many lines", nslides);
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

void
barf(Image *slide, int n)
{
	int fd;
	char path[64];

	snprint(path, sizeof path, "spit.%03d.bit", n);
	if((fd = create(path, OWRITE, 0644)) < 0)
		sysfatal("open: %r");
	writeimage(fd, slide, 0);
	close(fd);
}

Image*
addslide(void)
{
	++nslides;
	if(nodraw && nslides > 0){
		barf(slides[0], nslides);
		draw(slides[0], slides[0]->r, cols[Cbg], nil, ZP);
		return slides[0];
	}
	if(nslides >= Maxslides)
		sysfatal("too many slides");
	slides[nslides] = allocimage(display, screenr, screen->chan, 0, coldefs[Cbg]);
	if(slides[nslides] == nil)
		sysfatal("allocimage: %r");
	return slides[nslides];
}

void
renderpagenum(Image *b)
{
	char num[8];
	Point p;
	Rectangle r;
	Image *t;

	snprint(num, sizeof num, "%3d", nslides+1);
	r = pt_textrect(fnum, num);
	t = pt_textdraw(fnum, num, r, &opts);
	p = Pt(b->r.max.x - margin - Dx(r), b->r.max.y - margin - Dy(r));
	draw(b, rectaddpt(r, p), cols[Cfg], t, ZP);
	freeimage(t);
}

Point
rendertitle(Image *b, Point p, char *s)
{
	Rectangle r;
	Image *i;

	while(*s == ' ' || *s == '\t')
		s++;
	if(*s == 0)
		s = "";
	r = pt_textrect(ftitle, s);
	i = pt_textdraw(ftitle, s, r, &opts);
	draw(b, rectaddpt(r, p), cols[Cfg], i, ZP);
	freeimage(i);
	p.y += Dy(r);
	line(b, Pt(p.x, p.y), Pt(b->r.max.x - margin, p.y), 0, 0, ftitlelinesz, cols[Cfg], ZP);
	//p.y += Dy(r);
	return p;
}

Point
rendertext(Image *b, Point p, char *s, int rjust)
{
	Rectangle r;
	Image *i;

	r = pt_textrect(rjust ? frtext : ftext, s);
	if(strlen(s) > 0){
		i = pt_textdraw(rjust ? frtext : ftext, s, r, &opts);
		if(rjust)
			draw(b, rectaddpt(r, Pt(p.x + screenr.max.x - Dx(r) - margin, p.y)), cols[Cfg], i, ZP);
		else
			//draw(b, Rect(p.x, p.y, p.x+Dx(bol->r), p.y+Dy(bol->r)), bol, 0, ZP);
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
	br = Rect(p.x, p.y, p.x + maxw + 4*padding, p.y + maxh + 2*padding);
	draw(b, br, cols[Ccbg], nil, ZP);
	border(b, br, 2, cols[Ccbord], ZP);
	p.x += 2*padding;
	p.y += 2*padding;
	for(i = 0; i < lines->nlines; i++){
		t = pt_textdraw(ffixed, lines->lines[i], r[i], &opts);
		draw(b, rectaddpt(r[i], Pt(p.x+padding, p.y)), cols[Cfg], t, ZP);
		freeimage(t);
		p.y += Dy(r[i]);
	}
	p.x -= Dx(bol->r) + margin - 2*padding;
	return p;
}

Point
renderimage(Image *b, Point p, char *f, int tile)
{
	Image *i;
	char dim[64], buf[1024];
	double fx, fy;
	int x, n, maxx, maxy, w, h, fd, pfd[2];

	fd = open(f, OREAD);
	if(fd <= 0)
		sysfatal("open: %r");
	i = readimage(display, fd, 0);
	maxy = screenr.max.y - margin;
	maxx = screenr.max.x - margin;
	h = maxy - p.y;
	w = maxx - p.x;
	if(tile)
		w = (maxx - (tile - 1) * margin) / tile;
	if((w < Dx(i->r) || h < Dy(i->r))){
		if(pipe(pfd) < 0)
			sysfatal("pipe: %r");
		switch(fork()){
		case -1: sysfatal("fork: %r");
		case 0:
			dup(pfd[0], 0);
			dup(pfd[0], 1);
			close(pfd[0]);
			close(pfd[1]);
			fx = (double)w / Dx(i->r);
			fy = (double)h / Dy(i->r);
			if(fx < fy){
				snprint(dim, sizeof dim, "%f%%", fx*100);
				execl("/bin/resample", "resample", "-f", "catmullrom", "-x", dim, nil);
			}else{
				snprint(dim, sizeof dim, "%f%%", fy*100);
				execl("/bin/resample", "resample", "-f", "catmullrom", "-y", dim, nil);
			}
			sysfatal("execl: %r");
		default:
			close(pfd[0]);
		}
		seek(fd, 0, 0);
		while((n = read(fd, buf, sizeof buf)) > 0)
			if(write(pfd[1], buf, n) != n)
				sysfatal("write: %r");
		write(pfd[1], buf, 0);
		freeimage(i);
		if((i = readimage(display, pfd[1], 0)) == nil)
			sysfatal("readimage: %r");
		close(pfd[1]);
	}
	if(tile)
		x = p.x;
	else
		x = (maxx - p.x) / 2 - Dx(i->r) / 2;
	draw(b, rectaddpt(i->r, Pt(x, p.y)), i, nil, ZP);
	if(tile)
		p.x += Dx(i->r) + margin;
	if(!tile || maxx - p.x <= margin){
		p.x = margin;
		p.y += Dy(i->r) + margin;
	}
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
		if(margin < 0) error(f, line, "invalid 'margin' value");
	}else if(strcmp(k, "padding") == 0){
		padding = atoi(s);
		if(padding < 0) error(f, line, "invalid 'padding' value");
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
	else if(strcmp(k, "rtext.size") == 0)
		frtextsz = atof(s);
	else if(strcmp(k, "num.size") == 0)
		fnumsz = atof(s);
	else if(strcmp(k, "fixed.font") == 0)
		ffixedname = strdup(s);
	else if(strcmp(k, "fixed.size") == 0)
		ffixedsz = atof(s);
	else if(strcmp(k, "title.linesize") == 0)
		ftitlelinesz = atoi(s);
	else if(strcmp(k, "screen.width") == 0)
		screenr.max.x = atoi(s);
	else if(strcmp(k, "screen.height") == 0)
		screenr.max.y = atoi(s);
	else
		error(f, line, "unknown style key");		
}

void
setpapersize(void)
{
	screenr = Rect(0, 0, Dx(screen->r), Dy(screen->r));
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
	frtext  = loadsfont(ftextname ? ftextname : ftitlename, frtextsz);
	fnum  = loadsfont(ftextname ? ftextname : ftitlename, fnumsz);
	ffixed = loadsfont(ffixedname ? ffixedname : ftitlename, ffixedsz);
	initimages();
}

void
render(char *f)
{
	enum { Sstart, Scomment, Scontent, Slist, Squote, Scode };
	Biobuf *bp;
	char *l, *k;
	int n, s, ln;
	Image *b;
	Rune r;
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
Title:
			if(nslides == -1) /* all style parsed but not slide rendered yet */
				loadstyle(f);
			else
				renderpagenum(b);
			p = Pt(margin, margin);
			b = addslide();
			p = rendertitle(b, p, l+1);
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
				p = renderimage(b, p, l+2, 0);
			else if(l[0] == ';'){
				s = Scomment;
				break;
			}else{
				n = chartorune(&r, l);
				if(r == L'→')
					p = rendertext(b, p, l+n+1, 1);
				else if(r == L'¡'){
					n = strtol(l+n+1, &k, 10);
					p = renderimage(b, p, k+1, n);
				}else
					p = rendertext(b, p, l, 0);
			}
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
	renderpagenum(b);
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
	setpapersize();
	render(f);
	if(nodraw){
		addslide();
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
