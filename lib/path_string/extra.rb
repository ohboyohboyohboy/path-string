#!/usr/bin/ruby
# encoding: utf-8

=begin      ::about::
author:     Kyle Yetter <kcy5b@yahoo.com>
created on: June 02, 2011
purpose:    (program | library | utility script | ?)
summary:
loads:      files required by this
autoloads:  autoload entries in this (e.g. YAML(yaml))
=end

require 'path_string'
defined?(YAML) or autoload :YAML, 'yaml'
defined?(JSON) or autoload :JSON, 'json'

class Path
  def yaml( *args )
    case args.length
    when 0
      YAML.load_file( self )
    when 1
      serialized = YAML.dump( *args )
      open( 'w' ) { | f | f.write( serialized ) }
    else
      raise ArgumentError, "wrong number of arguments (#{ args.length } for 1)"
    end
  end

  def marshal( *args )
    case args.length
    when 0
      Marshal.load( read )
    when 1
      serialized = Marshal.dump( *args )
      open( 'wb' ) { | f | f.write( serialized ) }
    else
      raise ArgumentError, "wrong number of arguments (#{ args.length } for 1)"
    end
  end

  def json( *args )
    case args.length
    when 0
      JSON.parse( read )
    when 1
      serialized = JSON.pretty_generate( args.first )
      open( 'w' ) { | f | f.write( serialized ) }
    else
      raise ArgumentError, "wrong number of arguments (#{ args.length } for 1)"
    end
  end
end
