#include "ruby.h"
#include "ruby/util.h"
#include "ruby/encoding.h"

#ifdef __CYGWIN__
#undef DOSISH
#endif

#define unless( expr ) if ( !( expr ) )
#define until( expr ) while ( !( expr ) )
#define APPLY_TAINTING(value, reference)
#define RUBY_MAX_CHAR_LEN 16
#define STR_TMPLOCK FL_USER7
#define STR_NOEMBED FL_USER1
#define STR_SHARED  FL_USER2 /* = ELTS_SHARED */
#define STR_ASSOC   FL_USER3
#define STR_SHARED_P(s) FL_ALL(s, STR_NOEMBED|ELTS_SHARED)
#define STR_ASSOC_P(s)  FL_ALL(s, STR_NOEMBED|STR_ASSOC)
#define STR_NOCAPA  (STR_NOEMBED|ELTS_SHARED|STR_ASSOC)
#define STR_NOCAPA_P(s) (FL_TEST(s,STR_NOEMBED) && FL_ANY(s,ELTS_SHARED|STR_ASSOC))
#define STR_UNSET_NOCAPA(s) do {\
    if (FL_TEST(s,STR_NOEMBED)) FL_UNSET(s,(ELTS_SHARED|STR_ASSOC));\
} while (0)


#define STR_SET_NOEMBED(str) do {\
    FL_SET(str, STR_NOEMBED);\
    STR_SET_EMBED_LEN(str, 0);\
} while (0)
#define STR_SET_EMBED(str) FL_UNSET(str, STR_NOEMBED)
#define STR_EMBED_P(str) (!FL_TEST(str, STR_NOEMBED))
#define STR_SET_EMBED_LEN(str, n) do { \
    long tmp_n = (n);\
    RBASIC(str)->flags &= ~RSTRING_EMBED_LEN_MASK;\
    RBASIC(str)->flags |= (tmp_n) << RSTRING_EMBED_LEN_SHIFT;\
} while (0)

#define STR_SET_LEN(str, n) do { \
    if (STR_EMBED_P(str)) {\
    STR_SET_EMBED_LEN(str, n);\
    }\
    else {\
    RSTRING(str)->as.heap.len = (n);\
    }\
} while (0)

#if defined __CYGWIN__ || defined DOSISH
#define DOSISH_UNC
#define DOSISH_DRIVE_LETTER
#define isdirsep(x) ((x) == '/' || (x) == '\\\\')
#else
#define isdirsep(x) ((x) == '/')
#endif

#define istilde( x ) ( ( x ) == '~' )
#define isrelprefix( x ) ( !( isdirsep( x ) || istilde( x ) ) )
#define isdot( x ) ( ( x ) == '.' )

#if defined(DOSISH_UNC) || defined(DOSISH_DRIVE_LETTER)
#define skipprefix rb_path_skip_prefix
#else
#define skipprefix( path ) ( path )
#endif

#define strrdirsep rb_enc_path_last_separator

#ifndef CharNext        /* defined as CharNext[AW] on Windows. */
# if defined(DJGPP)
#   define CharNext(p) ((p) + mblen(p, MB_CUR_MAX))
# else
#   define CharNext(p) ((p) + 1)
# endif
#endif

#if USE_NTFS
#define istrailinggabage(x) ((x) == '.' || (x) == ' ')
#else
#define istrailinggabage(x) 0
#endif

#define DERIVE_PATH( var, cstr, len ) do {\
        var = rb_str_new5( self, cstr, len );\
        APPLY_TAINTING( var, self );\
      } while ( 0 );

#define lesser(a,b) (((a)>(b))?(b):(a))

#ifndef my_getcwd
#define my_getcwd() ruby_getcwd()
#endif

VALUE rb_cPath;


static const char path_collation[] = {
    '\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
    '\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
    '\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
    '\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
    /* ' '     '!'     '"'     '#'     '$'     '%'     '&'     ''' */
    '\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
    /* '('     ')'     '*'     '+'     ','     '-'     '.'     '/' */
    '\050', '\051', '\052', '\053', '\054', '\055', '\056', '\000',
    /* '0'     '1'     '2'     '3'     '4'     '5'     '6'     '7' */
    '\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
    /* '8'     '9'     ':'     ';'     '<'     '='     '>'     '?' */
    '\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
    /* '@'     'A'     'B'     'C'     'D'     'E'     'F'     'G' */
    '\100', '\101', '\102', '\103', '\104', '\105', '\106', '\107',
    /* 'H'     'I'     'J'     'K'     'L'     'M'     'N'     'O' */
    '\110', '\111', '\112', '\113', '\114', '\115', '\116', '\117',
    /* 'P'     'Q'     'R'     'S'     'T'     'U'     'V'     'W' */
    '\120', '\121', '\122', '\123', '\124', '\125', '\126', '\127',
    /* 'X'     'Y'     'Z'     '['     '\'     ']'     '^'     '_' */
    '\130', '\131', '\132', '\133', '\134', '\135', '\136', '\137',
    /* '`'     'a'     'b'     'c'     'd'     'e'     'f'     'g' */
    '\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
    /* 'h'     'i'     'j'     'k'     'l'     'm'     'n'     'o' */
    '\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
    /* 'p'     'q'     'r'     's'     't'     'u'     'v'     'w' */
    '\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
    /* 'x'     'y'     'z'     '{'     '|'     '}'     '~'   */
    '\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
    '\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
    '\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
    '\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
    '\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
    '\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
    '\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
    '\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
    '\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
    '\300', '\301', '\302', '\303', '\304', '\305', '\306', '\307',
    '\310', '\311', '\312', '\313', '\314', '\315', '\316', '\317',
    '\320', '\321', '\322', '\323', '\324', '\325', '\326', '\327',
    '\330', '\331', '\332', '\333', '\334', '\335', '\336', '\337',
    '\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
    '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
    '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
    '\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377'
};


#if USE_NTFS
static char *
ntfs_tail( const char *path )
{
    while ( *path == '.' ) path++;
    while ( *path && *path != ':' ) {
        if ( istrailinggabage( *path ) ) {
            const char *last = path++;
            while ( istrailinggabage( *path ) ) path++;
            if ( !*path || *path == ':' ) return ( char * )last;
        }
        else if ( isdirsep( *path ) ) {
            const char *last = path++;
            while ( isdirsep( *path ) ) path++;
            if ( !*path ) return ( char * )last;
            if ( *path == ':' ) path++;
        }
        else {
            path = CharNext( path );
        }
    }
    return ( char * )path;
}
#endif


#ifdef DOSISH_DRIVE_LETTER
static inline int
has_drive_letter( buf )
const char *buf;
{
    if ( ISALPHA( buf[0] ) && buf[1] == ':' ) {
        return 1;
    }
    else {
        return 0;
    }
}

static char*
getcwdofdrv( drv )
int drv;
{
    char drive[4];
    char *drvcwd, *oldcwd;

    drive[0] = drv;
    drive[1] = ':';
    drive[2] = '\0';

    /* the only way that I know to get the current directory
       of a particular drive is to change chdir() to that drive,
       so save the old cwd before chdir()
    */
    oldcwd = my_getcwd();
    if ( chdir( drive ) == 0 ) {
        drvcwd = my_getcwd();
        chdir( oldcwd );
        free( oldcwd );
    }
    else {
        /* perhaps the drive is not exist. we return only drive letter */
        drvcwd = strdup( drive );
    }
    return drvcwd;
}
#endif



static char *
chompdirsep( path )
const char *path;
{
    while ( *path ) {
        if ( isdirsep( *path ) ) {
            const char *last = path++;
            while ( isdirsep( *path ) ) path++;
            if ( !*path ) return ( char * )last;
        } else {
            path = CharNext( path );
        }
    }
    return ( char * )path;
}


static inline char *
skiproot( path )
const char *path;
{
#ifdef DOSISH_DRIVE_LETTER
    if ( has_drive_letter( path ) ) path += 2;
#endif
    while ( isdirsep( *path ) ) path++;
    return ( char * )path;
}


static VALUE rb_path_root_p(VALUE self) {

    char *path;
    StringValue( self );
    path = RSTRING_PTR( self );
    if ( ( RSTRING_LEN( self ) == 0 ) || !( *path ) ) {
        return (Qfalse);
    }
    path = skiproot( path );
    return (( !*path ? Qtrue : Qfalse ));
}


static int
rmext( p, l1, e )
const char *p, *e;
int l1;
{
    /* p = pointer to base name of a path
     * e = an extension string
     * l1 = the length of the basename, excluding any trailing separator
     */
    int l2;

    if ( !e ) return 0;

    l2 = strlen( e );
    if ( l2 == 2 && e[1] == '*' ) {  // if e == '_*' _ = any char
        unsigned char c = *e;        // c = _
        e = p + l1;                  // e = the last character
        do {
            if ( e <= p ) return 0;  // if e has moved back beyond the base, there's no extension
        } while ( *--e != c );       // stop if e points to _
        return e - p;                // return the length of the base without the extension
    }
    if ( l1 < l2 ) return l1;        // if the extension is longer than the base, there's no extension to look for

#if CASEFOLD_FILESYSTEM
#define fncomp strncasecmp
#else
#define fncomp strncmp
#endif
    if ( fncomp( p+l1-l2, e, l2 ) == 0 ) {
        return l1-l2;
    }
    return 0;
}


static VALUE
path_alloc( VALUE klass )
{
    NEWOBJ( path, struct RString );
    OBJSETUP( path, klass, T_STRING );

    path->as.heap.ptr = 0;
    path->as.heap.len = 0;
    path->as.heap.aux.capa = 0;

    return (VALUE) path;
}

static VALUE
path_new( VALUE klass, char *ptr, long len )
{
    VALUE path;

    if ( len < 0 ) {
        rb_raise( rb_eArgError, "negative string size (or size too big)" );
    }
    path = path_alloc( klass );

    if (len > RSTRING_EMBED_LEN_MAX) {
        RSTRING( path )->as.heap.aux.capa = len;
        RSTRING( path )->as.heap.ptr = ALLOC_N( char, len + 1 );
        STR_SET_NOEMBED( path );
    }
    else if ( len == 0 ) {
          ENC_CODERANGE_SET( path, ENC_CODERANGE_7BIT );
    }
    if ( ptr ) {
          memcpy( RSTRING_PTR( path ), ptr, len );
    }
    STR_SET_LEN( path, len );
    RSTRING_PTR( path )[len] = '\0';

    return path;
}


static VALUE
working_directory( VALUE klass )
{
    char *wd;
    VALUE path;

    if ( NIL_P( klass ) ) {
        klass = rb_cPath;
    }
    wd = my_getcwd();
    path = path_new( klass, wd, ( long ) strlen( wd ) );
    free( wd );
    OBJ_TAINT( path );

    return path;
}


static VALUE rb_path_clean_p( VALUE self ) {
    const char *path       = RSTRING_PTR( self );
    rb_encoding *enc       = rb_enc_get( self );
    const char *last_point = path + strlen( path );
    char *next_sep;

    if ( path == NULL || *path == '\0' ) {
        return Qfalse;
    }
    if ( strcmp( path, "." ) == 0 ) {
        return Qtrue;
    }
    if ( isdirsep( *path ) ) {
        path++;
    }
    if ( isdirsep( *path ) ) {
        return Qfalse;
    }

    while ( *path ) {
        next_sep = rb_enc_path_next( path, last_point, enc );

        switch ( next_sep - path ) {
        case 1:
            if ( *path == '.' ) {
                return Qfalse;
            }
            break;
        case 2:
            if ( strncmp( path, "..", 2 ) == 0 ) {
                return Qfalse;
            }
            break;
        default:
            break;
        }

        path = next_sep;
        if ( isdirsep( *path ) ) {
            path++;
            if ( !*path || isdirsep( *path ) ) {
                return Qfalse;
            }
        }
    }

    return Qtrue;
}


static VALUE rb_path_s_pwd(VALUE self) {

    return (working_directory( self ));
}


static VALUE rb_path_base( int argc, VALUE *argv, VALUE self )
{
    VALUE fext, basename;
    const char *name, *p, *name_end;
    rb_encoding *enc;
#if defined DOSISH_DRIVE_LETTER || defined DOSISH_UNC
    const char *root;
#endif
    int f, n;

    if ( rb_scan_args( argc, argv, "01", &fext ) == 1 ) {
        StringValue( fext );
    }
    StringValue( self );

    if ( RSTRING_LEN( self ) == 0 || !*( name = RSTRING_PTR( self ) ) )
        return self;

    enc      = rb_enc_get( self );
    name_end = name + RSTRING_LEN( self );
    name     = skipprefix( name );

#if defined DOSISH_DRIVE_LETTER || defined DOSISH_UNC
    root     = name;
#endif

    while ( isdirsep( *name ) ) {
        name++;
    }

    if ( !*name ) {
        p = name - 1;
        f = 1;
#if defined DOSISH_DRIVE_LETTER || defined DOSISH_UNC
        if ( name != root ) {
        }
#ifdef DOSISH_DRIVE_LETTER
        else if ( *p == ':' ) {
            p++;
            f = 0;
        }
#endif
#ifdef DOSISH_UNC
        else {
            p = "/";
        }
#endif
#endif
    } else {

        if ( !( p = strrdirsep( name, name_end, enc ) ) ) {
            p = name;
        } else {
            while ( isdirsep( *p ) ) p++;
        }
#if USE_NTFS
        n = ntfs_tail( p ) - p;
#else
        n = chompdirsep( p ) - p;
#endif
        if ( NIL_P( fext ) || !( f = rmext( p, n, StringValueCStr( fext ) ) ) ) {
            f = n;
        }
        if ( f == RSTRING_LEN( self ) ) return self;
    }
    DERIVE_PATH( basename, p, f );
    return basename;
}


static VALUE rb_path_expand( int argc, VALUE *argv, VALUE self )
{
    VALUE reference_path;
    VALUE expanded;
    VALUE expanded_as_path_object;

    StringValue( self );

    if ( rb_scan_args( argc, argv, "01", &reference_path ) == 0 ) {
        reference_path = working_directory( rb_cPath );
    }

    StringValue( reference_path );
    expanded = rb_file_expand_path( self, reference_path );

    expanded_as_path_object = rb_str_new_with_class( self, RSTRING_PTR( expanded ), RSTRING_LEN( expanded ) );

    APPLY_TAINTING( expanded_as_path_object, self );
    APPLY_TAINTING( expanded_as_path_object, reference_path );

    return expanded_as_path_object;
}


static VALUE rb_path_expand_bang( int argc, VALUE *argv, VALUE self )
{
    VALUE expanded = rb_path_expand( argc, argv, self );
    rb_funcall( self, rb_intern( "replace" ), 1, expanded );
    return self;
}


static VALUE rb_path_absolute_p(VALUE self) {

    char *path;
    StringValue( self );
    path = RSTRING_PTR( self );
    if ( isdirsep( *path ) || istilde( *path ) ) {
        return (Qtrue);
    }
    return (Qfalse);
}


static VALUE rb_path_relative_p(VALUE self) {

    char *path;
    StringValue( self );
    path = RSTRING_PTR( self );
    if ( isdirsep( *path ) || istilde( *path ) ) {
        return (Qfalse);
    }
    return (Qtrue);
}


static VALUE rb_path_dir(VALUE self) {

    const char *name, *root, *p, *name_end;
    rb_encoding *enc;
    VALUE dirname;

    enc      = rb_enc_get( self );
    name     = StringValueCStr( self );
    name_end = name + RSTRING_LEN( self );
    root     = skiproot( name );

#ifdef DOSISH_UNC
    if ( root > name + 1 && isdirsep( *name ) )
        root = skipprefix( name = root - 2 );
#else
    if ( root > name + 1 )
        name = root - 1;
#endif
    p = strrdirsep( root, name_end, enc );
    if ( !p ) {
        p = root;
    }
    if ( p == name ) {
        DERIVE_PATH( dirname, ".", 1 );
        return (dirname);
    }

#ifdef DOSISH_DRIVE_LETTER
    if ( has_drive_letter( name ) && isdirsep( *( name + 2 ) ) ) {
        const char *top = skiproot( name + 2 );
        dirname = rb_str_new5( self, name, 3 );
        rb_str_cat( dirname, top, p - top );
    }
    else
#endif
        DERIVE_PATH( dirname, name, p - name );

#ifdef DOSISH_DRIVE_LETTER
    if ( has_drive_letter( name ) && root == name + 2 && p - name == 2 )
        rb_str_cat( dirname, ".", 1 );
#endif
    return (dirname);
}



struct path_segment {
    char *ptr;
    long len;
};

#define CLEAN_STACK_SIZE 15
#define CLEAN_STACK_GROW 10

#define CLEAN_STACK_PUSH( str, n ) do {\
        if ( stack_size == 0 ) {\
          stack = ALLOC_N( struct path_segment *, CLEAN_STACK_SIZE );\
          stack_size = CLEAN_STACK_SIZE;\
        } else if ( stack_pos == stack_size ) {\
          stack_size += CLEAN_STACK_GROW;\
          REALLOC_N( stack, struct path_segment *, stack_size );\
        }\
        if ( stack_pos == stack_len ) {\
          stack[ stack_pos ] = ALLOC_N( struct path_segment, 1 );\
          stack_len++;\
        }\
        stack[ stack_pos ]->ptr = str;\
        stack[ stack_pos ]->len = n;\
        stack_pos++;\
      } while( 0 )

#define NEEDS_SEP( buf ) (\
        ( RSTRING_LEN( buf ) > 0 ) &&\
        !( isdirsep( RSTRING_PTR( buf )[ RSTRING_LEN( buf ) - 1 ] ) )\
      )

#define SINGLE_DOT_P( str, n ) (\
        ( n == 1 ) && ( *str == '.' )\
      )

#define DOUBLE_DOT_P( str, n ) (\
        ( n == 2 ) && ( strncmp( str, "..", 2 ) == 0 )\
      )

#define CLEAN_BUFF_JOIN( buf, ptr, len ) do {\
          if ( NEEDS_SEP( buf ) ) {\
                rb_str_buf_cat( buf, "/", 1 );\
          }\
          rb_str_buf_cat( buf, ptr, len );\
      } while ( 0 )

#define KEEP_LEADING_DOT     1
#define KEEP_TRAILING_SEP    2


static VALUE rb_path_clean(VALUE self) {
    char *seg_start;
    char *seg_end;
    char *seg_last;
    struct path_segment **stack = NULL;
    rb_encoding *enc = rb_enc_get(self);
    int stack_size = 0;
    int stack_len  = 0;
    int stack_pos = 0;
    int seg_size = 0;
    int absolute = 0;
    VALUE result_buffer = rb_str_buf_new( RSTRING_LEN( self ) );
    VALUE result;

    seg_start = RSTRING_PTR( self );
    seg_end   = skipprefix( seg_start );
    seg_last  = seg_start + strlen( seg_start );

    if ( istilde( *seg_end ) ) {
        seg_end = rb_enc_path_next( seg_end, seg_last, enc );
        rb_str_buf_cat( result_buffer, seg_start, ( seg_end - seg_start ) );
    } else if ( isdirsep( *seg_end ) ) {
        absolute = 1;
        rb_str_buf_cat( result_buffer, seg_start, ( seg_end - seg_start ) + 1 );
    }
    while ( isdirsep( *seg_end ) ) {
        seg_end++;
    }

    seg_start = seg_end;

    while ( *seg_start ) {
        seg_end  = rb_enc_path_next( seg_start, seg_last, enc );
        seg_size = seg_end - seg_start;

        if ( SINGLE_DOT_P( seg_start, seg_size ) ) {
        } else if ( DOUBLE_DOT_P( seg_start, seg_size ) ) {
            if ( stack_pos == 0 ) {
                if ( absolute ) {
                } else {
                    CLEAN_BUFF_JOIN( result_buffer, seg_start, seg_size );
                }
            } else {
                stack_pos--;
            }
        } else {
            CLEAN_STACK_PUSH( seg_start, seg_size );
        }
        seg_start = seg_end;
        while ( isdirsep( *seg_start ) ) {
            seg_start++;
        }
    }

    if ( stack ) {
        struct path_segment *seg;
        int i;

        for ( i = 0; i < stack_len; i++ ) {
            seg = stack[ i ];

            if ( i < stack_pos ) {
                CLEAN_BUFF_JOIN( result_buffer, seg->ptr, seg->len );
            }
            free( seg );
        }
        free( stack );
    }

    if ( RSTRING_LEN( result_buffer ) == 0 ) {
        rb_str_buf_cat( result_buffer, ".", 1 );
    }

    result = rb_str_new_with_class(self, RSTRING_PTR(result_buffer), RSTRING_LEN(result_buffer) );
    APPLY_TAINTING( result, self );
    return (result);
}


static VALUE rb_path_clean_bang(VALUE self) {

    if ( !RTEST( rb_path_clean_p( self ) ) ) {
        VALUE clean = rb_path_clean( self );
        rb_funcall( self, rb_intern( "replace" ), 1, clean );
    }

    return (self);
}


static VALUE rb_path_join_bang( int argc, VALUE *argv, VALUE self )
{
    long i;
    int taint = 0;
    VALUE tmp;
    const char *name;

    rb_path_clean_bang( self );
    if ( argc == 0 ) return self;

    for ( i = 0; i < argc; i++ ) {
        tmp = argv[ i ];
        name = StringValueCStr( tmp );

        rb_str_cat( self, "/", 1 );
        rb_str_cat2( self, name );

        if ( OBJ_TAINTED( tmp ) ) taint = 1;
    }

    rb_path_clean_bang( self );

    if ( taint ) OBJ_TAINT( self );
    return self;
}


static VALUE rb_path_join( int argc, VALUE *argv, VALUE self )
{
    VALUE path_copy = rb_str_dup( self );
    APPLY_TAINTING( path_copy, self );

    return rb_path_join_bang( argc, argv, path_copy );
}


static const char *const ruby_exts[] = {
    ".rb", DLEXT,
#ifdef DLEXT2
    DLEXT2,
#endif
    0
};


static VALUE rb_path_where_p(VALUE self, VALUE _lib_name) {
    VALUE lib_name = (_lib_name);

    VALUE match;

    StringValue( lib_name );
    rb_find_file_ext( &lib_name, ruby_exts );
    match = rb_find_file( lib_name );
    if ( RTEST( match ) ) {
        VALUE result;
        result = path_new( self, RSTRING_PTR( match ), RSTRING_LEN( match ) );
        APPLY_TAINTING( result, match );
        return (result);
    }
    return (match);
}


static int
path_cmp_n( const char *x, const char *y, long len )
{
    const char *p1 = x, *p2 = y;
    int tmp;

    while ( len-- ) {
        if ( ( tmp = path_collation[ ( unsigned )*p1++ ] - path_collation[ ( unsigned )*p2++ ] ) != 0 )
            return tmp;
    }
    return 0;
}


static VALUE rb_path_cmp(VALUE self, VALUE _other_path) {
    VALUE other_path = (_other_path);

    int shared_size, retval;
    StringValue( other_path );

    shared_size = lesser( RSTRING_LEN( self ), RSTRING_LEN( other_path ) );
    retval = path_cmp_n( RSTRING_PTR( self ), RSTRING_PTR( other_path ), shared_size );
    if ( retval == 0 ) {
        if ( RSTRING_LEN( self ) == RSTRING_LEN( other_path ) ) return (INT2FIX( 0 ));
        if ( RSTRING_LEN( self ) > RSTRING_LEN( other_path ) ) return (INT2FIX( 1 ));
        return (INT2FIX( -1 ));
    }
    if ( retval > 0 ) return (INT2FIX( 1 ));
    return (INT2FIX( -1 ));
}


static char *
path_locate_ext( char *path, char *path_end, rb_encoding *enc, int *ext_size )
{
    char *cursor;
    char *ext = NULL;

    cursor = strrdirsep( path, path_end, enc );
    if ( !cursor ) {
        cursor = path;
    }
    else {
        path = ++cursor;
    }

    /* cursor should now be at the first char of the base name of the path */
    while ( *cursor ) {
        if ( *cursor == '.' ) {
            ext = cursor;
        } else if ( isdirsep( *cursor ) ) {
            break;
        }

        cursor = CharNext( cursor );
    }

    if ( !ext || ext == path || ext + 1 == cursor ) {
        *ext_size = 0;
        return NULL;
    }

    *ext_size = cursor - ext;
    return ext;
}


static VALUE rb_path_extension(VALUE self) {
    VALUE extension;
    char *ext = NULL;
    char *name, *name_end;
    rb_encoding *enc;
    int ext_size = 0;

    enc      = rb_enc_get( self );
    name     = RSTRING_PTR( self );
    name_end = name + RSTRING_LEN( self );
    ext      = path_locate_ext( name, name_end, enc, &ext_size );

    if ( ext != NULL && ext_size > 0 ) {
        extension = rb_str_new( ext, ext_size );
    } else {
        extension = rb_str_new( 0, 0 );
    }

    APPLY_TAINTING( extension, self );
    return (extension);
}

#define ADD_SEG( ptr, len ) do { \
  VALUE path_seg = rb_str_new5( self, ( ptr ), ( len ) );\
  APPLY_TAINTING( path_seg, self );\
  rb_ary_push( segment_list, path_seg );\
} while ( 0 )

static VALUE rb_path_segments( VALUE self ) {
    char *seg_start   = RSTRING_PTR( self );
    char *seg_end     = skipprefix( seg_start );
    char *seg_last    = seg_start + strlen( seg_start );
    rb_encoding *enc  = rb_enc_get( self );

    int seg_size = 0;
    VALUE segment_list = rb_ary_new();

    if ( istilde( *seg_end ) ) {
        seg_end = rb_enc_path_next( seg_end, seg_last, enc );
        ADD_SEG( seg_start, seg_end - seg_start );
    } else if ( isdirsep( *seg_end ) ) {
        ADD_SEG( seg_start, ( seg_end - seg_start ) + 1 );
    }

    while ( isdirsep( *seg_end ) ) {
        seg_end++;
    }

    seg_start = seg_end;

    while ( *seg_start ) {
        seg_end = rb_enc_path_next( seg_start, seg_last, enc );
        seg_size = seg_end - seg_start;

        ADD_SEG( seg_start, seg_size );

        seg_start = seg_end;
        while ( isdirsep( *seg_start ) ) {
            seg_start++;
        }
    }

    if ( RARRAY_LEN( segment_list ) == 0 ) {
        ADD_SEG( ".", 1 );
    }

    APPLY_TAINTING( segment_list, self );
    return segment_list;
}

#undef ADD_SEG

static VALUE rb_path_descend(VALUE self) {

    char *cursor;
    char *path;
    VALUE current_path;

    RETURN_ENUMERATOR( self, 0, 0 );

    if ( !RTEST( rb_path_clean_p( self ) ) ) {
        self = rb_path_clean( self );
    }

    path   = RSTRING_PTR( self );
    cursor = path + RSTRING_LEN( self );

    do {
        DERIVE_PATH( current_path, path, cursor - path );
        rb_yield( current_path );

        do {
            cursor--;
        }
        while ( ( cursor > path ) && !isdirsep( *cursor ) );
    } while ( cursor > path );

    return (self);
}


static VALUE rb_path_ascend(VALUE self) {
    char *cursor;
    char *clean_path;
    char *last_point;
    char *path;
    VALUE current_path;
    VALUE path_obj;
    rb_encoding *enc  = rb_enc_get( self );

    RETURN_ENUMERATOR( self, 0, 0 );

    path       = RSTRING_PTR( self );

    if ( !rb_path_clean_p( self ) ) {
        path_obj = rb_path_clean( self );
        clean_path = RSTRING_PTR( path_obj );
    } else {
        clean_path = path;
    }

    cursor     = skiproot( clean_path );
    last_point = cursor + strlen( cursor );

    if ( cursor > clean_path ) {
        DERIVE_PATH( current_path, clean_path, cursor - clean_path );
        rb_yield( current_path );
    }

    while ( *cursor ) {
        cursor = rb_enc_path_next( cursor, last_point, enc );
        DERIVE_PATH( current_path, clean_path, cursor - clean_path );
        rb_yield( current_path );

        if ( *cursor ) {
            cursor++;
        }
    }

    return (self);
}


static VALUE rb_path_tilde_p(VALUE self) {

    if ( RSTRING_LEN( self ) > 0 ) {
        return( ( RSTRING_PTR( self )[ 0 ] == '~' ) ? Qtrue : Qfalse );
    } else {
        return (Qfalse);
    }
}



#ifdef __cplusplus
extern "C" {
#endif
    void Init_path_string_native() {
        rb_cPath = rb_define_class( "Path", rb_cString );

        rb_define_method(rb_cPath, "absolute?", (VALUE(*)(ANYARGS))rb_path_absolute_p, 0);
        rb_define_method(rb_cPath, "ascend", (VALUE(*)(ANYARGS))rb_path_ascend, 0);
        rb_define_method(rb_cPath, "base", (VALUE(*)(ANYARGS))rb_path_base, -1);
        rb_define_method(rb_cPath, "clean", (VALUE(*)(ANYARGS))rb_path_clean, 0);
        rb_define_method(rb_cPath, "clean!", (VALUE(*)(ANYARGS))rb_path_clean_bang, 0);
        rb_define_method(rb_cPath, "clean?", (VALUE(*)(ANYARGS))rb_path_clean_p, 0);
        rb_define_method(rb_cPath, "<=>", (VALUE(*)(ANYARGS))rb_path_cmp, 1);
        rb_define_method(rb_cPath, "descend", (VALUE(*)(ANYARGS))rb_path_descend, 0);
        rb_define_method(rb_cPath, "dir", (VALUE(*)(ANYARGS))rb_path_dir, 0);
        rb_define_method(rb_cPath, "expand", (VALUE(*)(ANYARGS))rb_path_expand, -1);
        rb_define_method(rb_cPath, "expand!", (VALUE(*)(ANYARGS))rb_path_expand_bang, -1);
        rb_define_method(rb_cPath, "extension", (VALUE(*)(ANYARGS))rb_path_extension, 0);
        rb_define_method(rb_cPath, "join", (VALUE(*)(ANYARGS))rb_path_join, -1);
        rb_define_method(rb_cPath, "join!", (VALUE(*)(ANYARGS))rb_path_join_bang, -1);
        rb_define_method(rb_cPath, "relative?", (VALUE(*)(ANYARGS))rb_path_relative_p, 0);
        rb_define_method(rb_cPath, "root?", (VALUE(*)(ANYARGS))rb_path_root_p, 0);
        rb_define_method(rb_cPath, "segments", (VALUE(*)(ANYARGS))rb_path_segments, 0);
        rb_define_singleton_method(rb_cPath, "pwd", (VALUE(*)(ANYARGS))rb_path_s_pwd, 0);
        rb_define_method(rb_cPath, "tilde?", (VALUE(*)(ANYARGS))rb_path_tilde_p, 0);
        rb_define_singleton_method(rb_cPath, "where?", (VALUE(*)(ANYARGS))rb_path_where_p, 1);

        rb_define_alias( rb_cPath, "/", "join" );
        rb_define_alias( rb_cPath, "~", "expand" );
        rb_define_alias( rb_cPath, "-@", "dir" );
        rb_define_alias( rb_cPath, "directory", "dir" );
        rb_define_alias( rb_cPath, "+@", "base" );


    }
#ifdef __cplusplus
}
#endif
