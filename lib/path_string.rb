#!/usr/bin/ruby
# encoding: utf-8

lib_directory = File.expand_path('..', __FILE__)
$LOAD_PATH.unshift(lib_directory) unless $LOAD_PATH.include?(lib_directory)

if RUBY_PLATFORM =~ /java/i
else
  require 'path_string/path_string_native.so'
end

require 'path_string/version'

class Path < String
  def self.constant_missing( name )
    case name.to_s
    when "Etc" then require 'etc'
    when "Mime" then require 'mime'
    else super
    end
  end

  autoload :List, 'path_string/list'

  # :stopdoc:
  if RUBY_VERSION < "1.9"
    TO_PATH = :to_str
  else
    # to_path is implemented so Path objects are usable with File.open, etc.
    TO_PATH = :to_path
  end
  # :startdoc:

  def inspect # :nodoc:
    "Path( #{ super } )"
  end

  DOUBLE_DOT = '..'.freeze
  SINGLE_DOT = '.'.freeze
  DOTS = [ DOUBLE_DOT, SINGLE_DOT ].freeze

  def +( str )
    self.class.new( super( str ) )
  end

  alias ext extension

  def expand_tilde
    tilde? ? expand : dup
  end

  def expand_tilde!
    tilde? ? expand! : self
  end

  def real
    path = expand
    loop do
      if path.symlink?
        path = path.readlink.expand( path.dir )
      elsif link = path.descend.find { | d | d.symlink? }
        tail = path.relative( link )
        path = link.readlink.expand( link.dir ) / tail
      else
        break
      end
    end
    return path
  end

  def parent
    self / '..'
  end

  # #mountpoint? returns +true+ if <tt>self</tt> points to a mountpoint.
  def mountpoint?
    begin
      stat1 = expand_tilde.lstat
      stat2 = expand_tilde.parent.lstat
      stat1.dev == stat2.dev && stat1.ino == stat2.ino ||
      stat1.dev != stat2.dev
    rescue Errno::ENOENT
      false
    end
  end


  #
  # Returns the children of the directory (files and subdirectories, not
  # recursive) as an array of Path objects.  By default, the returned
  # pathnames will have enough information to access the files.  If you set
  # +with_directory+ to +false+, then the returned pathnames will contain the
  # filename only.
  #
  # For example:
  #   p = Path("/usr/lib/ruby/1.8")
  #   p.children
  #       # -> [ Path:/usr/lib/ruby/1.8/English.rb,
  #              Path:/usr/lib/ruby/1.8/Env.rb,
  #              Path:/usr/lib/ruby/1.8/abbrev.rb, ... ]
  #   p.children(false)
  #       # -> [ Path:English.rb, Path:Env.rb, Path:abbrev.rb, ... ]
  #
  # Note that the result never contain the entries <tt>.</tt> and <tt>..</tt> in
  # the directory because they are not children.
  #
  # This method has existed since 1.8.1.
  #
  def children( with_directory = ( self != '.' ) )
    ( Dir.entries( expand_tilde ) - DOTS ).map do | entry |
      with_directory ? join( entry ) : self.class.new( entry )
    end
  end

  #
  # #relative returns a relative path from the argument to the
  # receiver.  If +self+ is absolute, the argument must be absolute too.  If
  # +self+ is relative, the argument must be relative too.
  #
  # #relative doesn't access the filesystem.  It assumes no symlinks.
  #
  # ArgumentError is raised when it cannot find a relative path.
  #
  # This method has existed since 1.8.1.
  #
  def relative( reference = Path.pwd )
    reference = Path( reference )

    pair = [ self, reference ].map! do | path |
      path.expand.split( File::Separator ).tap do | list |
        if list.empty? then list << Path( File::Separator )
        elsif list.first.empty? then list.first.replace( File::Separator )
        end
      end
    end

    target_list, reference_list = pair
    while target_list.first == reference_list.first
      target_list.shift
      reference_list.shift or break
    end

    relative_list = Array.new( reference_list.length, '..' )
    relative_list.empty? and target_list.empty? and relative_list << '.'
    relative_list.concat( target_list ).compact!
    return self.class.new( relative_list.join( File::Separator ) )
  end

  def mime_type
    require 'mime/path'
    test( ?e ) ? Mime::Type.for_file( self ) : nil
  end

  #
  # Time (in seconds) since the file was last modified
  #
  def age() Time.now - mtime end

  def user
    Etc.getpwuid( File.stat( self ).uid ).name
  end

  def user_id
    File.stat( expand_tilde ).uid
  end

  def group
    Etc.getgrgid( File.stat( expand_tilde ).gid ).name
  end

  def group_id
    File.stat( expand_tilde ).gid
  end

  def mime_type
    test( ?e ) ? Mime::Type.for_file( expand_tilde ) : nil
  end

end

class Path    # * IO *
  #
  # #each_line iterates over the line in the file.  It yields a String object
  # for each line.
  #
  # This method has existed since 1.8.1.
  #
  def each_line( *args, &block ) # :yield: line
    IO.foreach( self.expand_tilde, *args, &block )
  end

  # See <tt>IO.read</tt>.  Returns all the bytes from the file, or the first +N+
  # if specified.
  def read( *args ) IO.read( expand_tilde, *args ) end

  def write( content, *args )
    open( 'wb', *args ) { | io | io.write( content ) }
  end

  # See <tt>IO.readlines</tt>.  Returns all the lines from the file.
  def readlines( *args ) IO.readlines( expand_tilde, *args ) end

  # See <tt>IO.sysopen</tt>.
  def sysopen( *args ) IO.sysopen( expand_tilde, *args ) end
end


class Path    # * File *
  # See <tt>File.atime</tt>.  Returns last access time.
  def atime() File.atime( expand_tilde ) end

  # See <tt>File.ctime</tt>.  Returns last (directory entry, not file) change time.
  def ctime() File.ctime( expand_tilde ) end

  # See <tt>File.mtime</tt>.  Returns last modification time.
  def mtime() File.mtime( expand_tilde ) end

  # See <tt>File.chmod</tt>.  Changes permissions.
  def chmod( mode ) File.chmod( mode, expand_tilde ) end

  # See <tt>File.lchmod</tt>.
  def lchmod( mode ) File.lchmod( mode, expand_tilde ) end

  # See <tt>File.chown</tt>.  Change owner and group of file.
  def chown( owner, group ) File.chown( owner, group, expand_tilde ) end

  # See <tt>File.lchown</tt>.
  def lchown( owner, group ) File.lchown( owner, group, expand_tilde ) end

  def mode
    File.stat( self ).mode
  end

  def mode=( mode )
    File.chmod( mode, self )
  end

  # See <tt>File.fnmatch</tt>.  Return +true+ if the receiver matches the given
  # pattern.
  def fnmatch( pattern, *args ) File.fnmatch( pattern, self, *args ) end

  # See <tt>File.fnmatch?</tt> (same as #fnmatch).
  def fnmatch?( pattern, *args ) File.fnmatch?( pattern, self, *args ) end

  # See <tt>File.ftype</tt>.  Returns "type" of file ("file", "directory",
  # etc).
  def ftype() File.ftype( expand_tilde ) end

  # See <tt>File.link</tt>.  Creates a hard link.
  def make_link( old ) File.link( old, expand_tilde ) end

  # See <tt>File.open</tt>.  Opens the file for reading or writing.
  def open( *args, &block ) # :yield: file
    File.open( expand_tilde, *args, &block )
  end

  # See <tt>File.readlink</tt>.  Read symbolic link.
  def readlink() self.class.new( File.readlink( expand_tilde ) ) end

  # See <tt>File.rename</tt>.  Rename the file.
  def rename( to ) File.rename( expand_tilde, to ) end

  # See <tt>File.stat</tt>.  Returns a <tt>File::Stat</tt> object.
  def stat() File.stat( expand_tilde ) end

  # See <tt>File.lstat</tt>.
  def lstat() File.lstat( expand_tilde ) end

  # See <tt>File.symlink</tt>.  Creates a symbolic link.
  def make_symlink( old ) File.symlink( old, expand_tilde ) end

  # See <tt>File.truncate</tt>.  Truncate the file to +length+ bytes.
  def truncate( length ) File.truncate( expand_tilde, length ) end

  # See <tt>File.utime</tt>.  Update the access and modification times.
  def utime( atime, mtime ) File.utime( atime, mtime, expand_tilde ) end

  def create_link( name )
    File.symlink( expand_tilde, name )
  end

  def self.null
    case RUBY_PLATFORM
    when /mswin/i
      new( 'NUL' )
    when /amiga/i
      new( 'NIL:' )
    when /openvms/i
      new( 'NL:' )
    else
      new( '/dev/null' )
    end
  end

  include FileTest

  for method in ::FileTest.private_instance_methods
    method = ::FileTest.instance_method( method )
    case method.arity
    when 2
      class_eval( <<-END, __FILE__, __LINE__ + 1 )
        def #{ method.name }( other )
          super( expand_tilde, Path( other ) )
        end
      END
    when 1
      class_eval( <<-END, __FILE__, __LINE__ + 1 )
        def #{ method.name }
          super( expand_tilde )
        end
      END
    else
      warn( "#{ method.inspect } has not been written into Path as it has an unexpected arity" )
    end
  end

  alias link? symlink?

  def test( code, arg = nil )
    arg ? Kernel.test( code, expand_tilde, arg ) : Kernel.test( code, expand_tilde )
  end

  #
  # Returns true if the file has been modified since +time+
  def modified_since?( time )
    mtime > time
  end



end


class Path    # * Dir *

  # Path#chdir is *obsoleted* at 1.8.1.
  def chdir
    if block_given?
      Dir.chdir( expand_tilde ) { yield }
    else
      Dir.chdir( expand_tilde )
    end
  end

  alias cd chdir
  alias enter chdir

  def chroot
    Dir.chroot( expand_tilde )
  end

  # Return the entries (files and subdirectories) in the directory, each as a
  # Path object.
  def entries() Dir.entries( expand_tilde ).map { |f| self.class.new( f ) } end

  # Iterates over the entries (files and subdirectories) in the directory.  It
  # yields a Path object for each entry.
  #
  # This method has existed since 1.8.1.
  def each_entry( &block ) # :yield: p
    Dir.foreach( expand_tilde ) { |f| yield self.class.new( f ) }
  end

  # Path#dir_foreach is *obsoleted* at 1.8.1.
  def dir_foreach( *args, &block )
    warn "Path#dir_foreach is obsoleted.  Use Path#each_entry."
    each_entry( *args, &block )
  end

  # See <tt>Dir.mkdir</tt>.  Create the referenced directory.
  def mkdir( *args ) Dir.mkdir( expand_tilde, *args ) end

  # See <tt>Dir.rmdir</tt>.  Remove the referenced directory.
  def rmdir() Dir.rmdir( expand_tilde ) end

  # See <tt>Dir.open</tt>.
  def opendir( &block ) # :yield: dir
    Dir.open( expand_tilde, &block )
  end
end



class Path    # * FileUtils *
  autoload :FileUtils, 'fileutils'

  def rename!( target_pattern )
    destination = transform( target_pattern )
    rename( destination )
  end

  def make!
    Dir.mkdir( expand_tilde )
  end

  # See <tt>FileUtils.mkpath</tt>.  Creates a full path, including any
  # intermediate directories that don't yet exist.
  def mkpath
    FileUtils.mkpath( expand_tilde )
  end

  # See <tt>FileUtils.rm_r</tt>.  Deletes a directory and all beneath it.
  def rmtree
    # The name "rmtree" is borrowed from File::Path of Perl.
    # File::Path provides "mkpath" and "rmtree".
    FileUtils.rm_r( expand_tilde )
  end
end


class Path    # * mixed *
  # Removes a file or directory, using <tt>File.unlink</tt> or
  # <tt>Dir.unlink</tt> as necessary.
  def unlink()
    begin
      Dir.unlink expand_tilde
    rescue Errno::ENOTDIR
      File.unlink expand_tilde
    end
  end
  alias delete unlink
end

class Path
  def self.join( base, *args )
    new( base ).join!( *args )
  end

  def self.cd( *args )
    if block_given?
      Dir.chdir( *args ) do |d|
        yield( new( d ) )
      end
    else
      Dir.chdir( *args )
    end
  end

  def self.[] *args
    args = [ args ].flatten!
    flags = Integer === args.last ? args.pop : 0
    args.inject( [] ) do | paths, pattern |
      pattern = new( pattern )
      pattern.tilde? and pattern.expand!
      paths.concat(
        pattern.glob( flags )
      )
    end
  end

  def self.glob( pattern, *args )
    pattern = new( pattern ).expand_tilde

    if block_given?
      pattern.glob( *args ) { | match | yield( match ) }
    else
      pattern.glob( *args )
    end
  end

  def glob( *args )
    args  = [ args ].flatten!
    flags = Integer === args.last ? args.pop : 0

    pattern = ( args.empty? ? self : join( *args ) ).expand_tilde

    matches = Dir.glob( pattern, flags ).map! { | p | self.class.new( p ) }
    if block_given?
      for match in matches
        yield( match )
      end
    end

    return matches
  end

  def hidden?
    base[ 0 ] == ?.
  end

  def leaf?
    type == :directory ? Dir.entries( self ).length == 2 : true
  end

  def files
    children.delete_if do | child |
      not child.file?
    end
  end

  def subdirectories
    children.delete_if do | child |
      not child.directory?
    end
  end

  alias directories subdirectories

  def child_of?( path )

    self.ascend { |pt| return true if pt == path }
    return false
  end

  def is_parent_of?( path )
    return path.is_child_of?( expand_tilde )
  end

  def extname=( ext )
    ( ext = '.' << ext ) unless ext[ 0 ] == ?.
    chomp!( extname )
    self << ext
  end


  def up( n = 1 )
    t = self
    n.times { t = t.parent }
    return t
  end

  def up!( n = 1 )
    replace( up( n ) )
  end

  def ftype
    File.ftype( expand_tilde ).to_sym
  end

  alias type ftype

  def copy_to( dest, options = {} )
    FileUtils.copy( expand_tilde, dest, options )
  end

  if defined?( on_demand )
    on_demand :mime_type, 'mime/path'
  end
end


class Path
  def split( *args )
    args.empty? or return( super )
    ::File.split( self ).map! { | t | self.class.new( t ) }
  end

  def chomp_ext( mask = '.*' )
    ext = extension
    File.fnmatch?( mask, ext ) ? chomp( ext ) : dup
  end

  def chomp_ext!( mask = '.*' )
    ext = extension
    File.fnmatch?( mask, ext ) ? chomp!( ext ) : nil
  end

  def swap_ext( new_ext = nil )
    new_ext or return( chomp_ext )
    stripped = chomp_ext
    new_ext[ 0 ] == ?. or stripped << ?.
    stripped << new_ext
  end

  def swap_ext!( new_ext = nil )
    new_ext or return( chomp_ext! )
    chomp_ext!
    new_ext[ 0 ] == ?. or self << ?.
    self << new_ext
  end

  def transform( spec = nil, &block )
    return self if spec.nil?
    result = self.class.new( '' )

    pattern = %r`
        %  \{ [^}]* \} -? \d* [sdpfnixX%]
      | %-? \d+ [di]
      | % .
      | [^%]+
    `x

    spec.scan( pattern ) do | frag |
      case frag
      when '%f' then result << base
      when '%n' then result << base( '.*' )
      when '%d' then result << dir
      when '%x' then result << extension
      when '%X' then result << chomp_ext
      when '%p' then result << self
      when '%s' then result << ( File::ALT_SEPARATOR || File::SEPARATOR )
      when '%-' then # do nothing
      when '%%' then result << ?%
      when /%(-?\d+)d/
        slice_size = $1.to_i
        slice =
          case slice_size <=> 0
          when 1  then segments.first( slice_size )
          when -1 then segments.last( slice_size.abs )
          else %w( '.' )
          end
        result << File.join( *slice )

      when /%(-?\d*)i/
        result << segments[ $1.to_i ]

      when /^%\{([^}]*)\}(\d*[dpfnxX])/
        patterns, operator = $1, $2
        result_segment = transform( "%#{ operator }" )

        for pair in patterns.split( ';' )
          pattern, replacement = pair.split( ',', 2 )
          pattern = Regexp.new( pattern )
          if replacement == '*' and block_given?
            result_segment.gsub!( pattern ) { | *args | yield( *args ) }
          elsif replacement
            result_segment.gsub!( pattern, replacement )
          else
            result_segment.gsub!( pattern, '' )
          end
        end

        result << result_segment
      when /^%/
        fail ArgumentError, "Unknown pathmap specifier #{frag} in '#{spec}'"
      else result << frag
      end
    end

    result
  end

end

class Path  # iterators
  Prune = Class.new( Exception )

  def self.prune
    raise Prune
  end

  def walk
    block_given? or return enum_for( :walk )
    base = expand_tilde
    path_stack = [ base ]
    while path = path_stack.shift
      begin
        is_dir = path.lstat.directory?
        yield( path )
        is_dir and
          begin
            path_stack.concat( path.children )
          rescue Errno::ENOENT, Errno::EACCES
          end
      rescue Prune # => skip before adding children
      end
    end
    return( self )
  end

  def self.search( d = '.', *dirs )
    block_given? or return enum_for( __method__, d, *dirs )
    dirs = [ d, dirs ].flatten!.inject( [] ) do | list, pattern |
      list.concat( new( pattern ).expand_tilde.glob )
    end

    dirs.inject( [] ) do | results, dir |
      results.concat( dir.search { | path | yield( path ) } )
    end
  end

  def search
    block_given? or return enum_for( :search )
    walk.select { | path | yield( path ) }
  end

  def prune
    raise Prune
  end

  def is_binary_data?
    ( self.count( "^ -~", "^\r\n" ).fdiv( length ) > 0.3 || index( "\x00" ) ) unless empty?
  end
end

class String
  def to_path
    Path(self)
  end
end

module Kernel
  def Path( path ) # :doc:
    Path === path ? path : Path.new( path.to_s )
  end

  module_function :Path
end

if defined?(Ruby)
  module Ruby
    def self.where?( library )
      Path.where?( library )
    end

    def self.glob( pattern, for_require = true )
      result = []
      lib_pattern =
        Path( pattern =~ /\.(rbw?|so)$/i ? pattern : "#{ pattern }.{rbw,rb,so}" )

      for dir in $LOAD_PATH
        ( dir = Path( dir ) ).directory? or next

        lib_pattern.expand( dir ).glob do | lib |
          lib = lib.relative( dir )
          for_require and lib.chomp_ext!
          result << ( block_given? ? yield( lib, dir ) : lib )
        end
      end

      result = result.sort_by { | lib | lib } # <- `path' sort since sorts as strings by default
      return( result.uniq! or result )
    end
  end
end

if defined?(Ruby::PathGroup)
  module Ruby::PathGroup
    module_function

    def new_path( base, *parts )
      Path( base ).join( *parts ).expand!
    end
  end
end
