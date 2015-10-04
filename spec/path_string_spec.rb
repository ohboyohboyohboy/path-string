#!/usr/bin/ruby
# encoding: utf-8
#
# author: Kyle Yetter
#

require 'path_string'

describe( Path ) do

  context "path segments" do

    it "provides you with the extension name of the path" do
      Path( "/x/y/z.txt" ).ext.should       == ".txt"
      Path( "/x/y/.hidden" ).ext.should     == ""
      Path( "/x/y/.hidden.wav" ).ext.should == ".wav"

      Path( "z.txt" ).ext.should            == ".txt"
      Path( ".hidden" ).ext.should          == ''
      Path( ".hidden.wav" ).ext.should      == '.wav'
    end

    it "provides you with the base name of the path" do
      Path( "/x/y/z.txt" ).base.should       == "z.txt"
      Path( "/x/y/.hidden" ).base.should     == ".hidden"
      Path( "/x/y/.hidden.wav" ).base.should == ".hidden.wav"

      Path( "z.txt" ).base.should            == "z.txt"
      Path( ".hidden" ).base.should          == '.hidden'
      Path( ".hidden.wav" ).base.should      == '.hidden.wav'
    end

    it "provides you with the base name of the path with an extension pattern removed" do
      Path( "/x/y/z.txt" ).base( ".txt" ).should       == "z"
      Path( "/x/y/z.txt" ).base( ".wav" ).should       == "z.txt"
      Path( "/x/y/z.txt" ).base( ".*" ).should         == "z"
    end

    it "provides you with the parent directory name of a path" do
      Path( "/x/y/z.txt" ).dir.should       == "/x/y"
      Path( "/x/y/.hidden" ).dir.should     == "/x/y"
      Path( "/x/y/.hidden.wav" ).dir.should == "/x/y"

      Path( "z.txt" ).dir.should            == "."
      Path( ".hidden" ).dir.should          == '.'
      Path( ".hidden.wav" ).dir.should      == '.'
    end

    it "provides you with the individual segments of the path" do
      Path( "/a/b/c" ).segments.should     == %w( / a b c )
      Path( ".//x/y.txt" ).segments.should == %w( . x y.txt )

      Path( "/a/b/c".taint ).segments.should be_tainted
      Path( "/a/b/c".taint ).segments.all? { | i | i.tainted? }.should be_true
    end

    it "aliases the unary + operator to provide base name" do
      (+Path( "/x/y/z.txt" )).should        == "z.txt"
      (+Path( "/x/y/.hidden" )).should      == ".hidden"
      (+Path( "/x/y/.hidden.wav" )).should  == ".hidden.wav"

      (++Path( "z.txt" )).should            == "z.txt"
      (++Path( ".hidden" )).should          == '.hidden'
      (++Path( ".hidden.wav" )).should      == '.hidden.wav'
    end

    it "aliases the unary - operator to provide directory name" do
      (-Path( "/x/y/z.txt" )).should       == "/x/y"
      (-Path( "/x/y/.hidden" )).should     == "/x/y"
      (-Path( "/x/y/.hidden.wav" )).should == "/x/y"
      (--Path( "z.txt" )).should           == "."
      (-Path( ".hidden" )).should          == '.'
      (--Path( "./x/.hidden.wav" )).should == '.'
    end

  end

  context "path form manipulation" do
    it "simplifies paths containing multiple slashes and dot references" do
      paths   = %w( /y/.././x   .//  ./x/../../  //../../x/../.. ).map { | str | Path( str ) }
      cleaned = %w( /x          .    ..          / )

      paths.zip( cleaned ) do | dirty, clean |
        cleaned_path = dirty.clean
        cleaned_path.should == clean
        cleaned_path.should_not be( dirty ) # different object identity
        dirty.taint.clean.should be_tainted
      end
    end

    it "simplifies paths containing multiple slashes and dot references in place" do
      paths   = %w( /y/.././x   .//  ./x/../../  //../../x/../.. ).map { | str | Path( str ) }
      cleaned = %w( /x          .    ..          / )

      paths.zip( cleaned ) do | dirty, clean |
        cleaned_path = dirty.clean!
        cleaned_path.should == clean
        cleaned_path.should be( dirty ) # same object identity for in place clean!
      end
    end

    it "removes any extension name from the path" do
      Path( "name.txt" ).chomp_ext.should     == "name"
      Path( "name.new.txt" ).chomp_ext.should == "name.new"
    end

    it "removes a given extension pattern from the path" do
      Path( "name.txt" ).chomp_ext( ".txt" ).should == "name"
      Path( "name.txt" ).chomp_ext( ".wav" ).should == "name.txt"
      Path( "name.txt" ).chomp_ext( ".*" ).should   == "name"
    end

    it "exchanges one extension for another" do
      Path( "name.wav" ).swap_ext( ".mp3" ).should == "name.mp3"
    end
  end
end
