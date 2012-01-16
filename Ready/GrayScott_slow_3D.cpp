/*  Copyright 2011, 2012 The Ready Bunch

    This file is part of Ready.

    Ready is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Ready is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Ready. If not, see <http://www.gnu.org/licenses/>.         */

// local:
#include "GrayScott_slow_3D.hpp"
#include "utils.hpp"

// stdlib:
#include <stdlib.h>
#include <math.h>

// STL:
#include <stdexcept>
#include <cassert>
using namespace std;

// VTK:
#include <vtkImageData.h>

GrayScott_slow_3D::GrayScott_slow_3D()
{
    this->timestep = 1.0f;
    this->rule_name = "Gray-Scott";
    this->r_a = 0.082f;
    this->r_b = 0.041f;
    // for spots:
    this->k = 0.064f;
    this->f = 0.035f;
}

GrayScott_slow_3D::~GrayScott_slow_3D()
{
    this->DeleteBuffers();
}

void GrayScott_slow_3D::DeleteBuffers()
{
    for(int i=0;i<(int)this->buffer_images.size();i++)
    {
        if(this->buffer_images[i])
            this->buffer_images[i]->Delete();
    }
}

void GrayScott_slow_3D::Allocate(int x,int y,int z,int nc)
{
    if(nc!=2) throw runtime_error("GrayScott_slow_3D::Allocate : this implementation is for 2 chemicals only");
    BaseRD::Allocate(x,y,z,2);
    // also allocate our buffers
    this->DeleteBuffers();
    this->buffer_images.resize(2);
    for(int i=0;i<2;i++)
        this->buffer_images[i] = AllocateVTKImage(x,y,z);
}

void GrayScott_slow_3D::Update(int n_steps)
{
    const int X = this->GetX();
    const int Y = this->GetY();
    const int Z = this->GetZ();

    // take approximately n_steps
    for(int iStepPair=0;iStepPair<(n_steps+1)/2;iStepPair++)
    {
        for(int iStep=0;iStep<2;iStep++)
        {
            float *old_a,*new_a,*old_b,*new_b;
            switch(iStep) {
                case 0: old_a = static_cast<float*>(this->images[0]->GetScalarPointer()); 
                        old_b = static_cast<float*>(this->images[1]->GetScalarPointer()); 
                        new_a = static_cast<float*>(this->buffer_images[0]->GetScalarPointer()); 
                        new_b = static_cast<float*>(this->buffer_images[1]->GetScalarPointer()); 
                        break;
                case 1: old_a = static_cast<float*>(this->buffer_images[0]->GetScalarPointer()); 
                        old_b = static_cast<float*>(this->buffer_images[1]->GetScalarPointer()); 
                        new_a = static_cast<float*>(this->images[0]->GetScalarPointer()); 
                        new_b = static_cast<float*>(this->images[1]->GetScalarPointer()); 
                        break;
            }

            for(int x=0;x<X;x++)
            {
                int x_prev = (x-1+X)%X;
                int x_next = (x+1)%X;
                for(int y=0;y<Y;y++)
                {
                    int y_prev = (y-1+Y)%Y;
                    int y_next = (y+1)%Y;
                    for(int z=0;z<Z;z++)
                    {
                        int z_prev = (z-1+Z)%Z;
                        int z_next = (z+1)%Z;

                        float aval = *vtk_at(old_a,x,y,z,X,Y);
                        float bval = *vtk_at(old_b,x,y,z,X,Y);

                        // compute the Laplacians of a and b
                        // 7-point stencil:
                        float dda = *vtk_at(old_a,x,y_prev,z,X,Y) +
                                    *vtk_at(old_a,x,y_next,z,X,Y) +
                                    *vtk_at(old_a,x_prev,y,z,X,Y) + 
                                    *vtk_at(old_a,x_next,y,z,X,Y) +
                                    *vtk_at(old_a,x,y,z_prev,X,Y) + 
                                    *vtk_at(old_a,x,y,z_next,X,Y) +
                                    - 6*aval;
                        float ddb = *vtk_at(old_b,x,y_prev,z,X,Y) +
                                    *vtk_at(old_b,x,y_next,z,X,Y) +
                                    *vtk_at(old_b,x_prev,y,z,X,Y) + 
                                    *vtk_at(old_b,x_next,y,z,X,Y) +
                                    *vtk_at(old_b,x,y,z_prev,X,Y) + 
                                    *vtk_at(old_b,x,y,z_next,X,Y) +
                                    - 6*bval; 

                        // compute the new rate of change of a and b
                        float da = this->r_a * dda - aval*bval*bval + this->f*(1-aval);
                        float db = this->r_b * ddb + aval*bval*bval - (this->f+this->k)*bval;

                        // apply the change
                        *vtk_at(new_a,x,y,z,X,Y) = aval + this->timestep * da;
                        *vtk_at(new_b,x,y,z,X,Y) = bval + this->timestep * db;
                    }
                }
            }

        }
        this->timesteps_taken+=2;
    }
    this->images[0]->Modified();
    this->images[1]->Modified();
}

void GrayScott_slow_3D::InitWithBlobInCenter()
{
    const int X = this->GetX();
    const int Y = this->GetY();
    const int Z = this->GetZ();

    vtkImageData *a_image = this->images[0];
    vtkImageData *b_image = this->images[1];
    float* a_data = static_cast<float*>(a_image->GetScalarPointer());
    float* b_data = static_cast<float*>(b_image->GetScalarPointer());

    for(int x=0;x<X;x++)
    {
        for(int y=0;y<Y;y++)
        {
            for(int z=0;z<Z;z++)
            {
                if(hypot3(x-X/2,(y-Y/2)/1.5,z-Z/2)<=frand(2.0f,5.0f)) // start with a uniform field with an approximate circle in the middle
                {
                    *vtk_at(a_data,x,y,z,X,Y) = 0.0f;
                    *vtk_at(b_data,x,y,z,X,Y) = 1.0f;
                }
                else 
                {
                    *vtk_at(a_data,x,y,z,X,Y) = 1.0f;
                    *vtk_at(b_data,x,y,z,X,Y) = 0.0f;
                }
            }
        }
    }
    a_image->Modified();
    b_image->Modified();
    this->timesteps_taken = 0;
}