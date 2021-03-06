/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright held by original author
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

Description
    Leaf of the binary tree. 
    The chemPoint stores the composition 'phi', the mapping of this
    composition Rphi, the mapping gradient matrix A and the matrix describing
    the Ellipsoid Of Accuracy (EOA).
   
  1)When the chemPoint is created the region of accuracy is approximated by
  	an ellipsoid E centered in 'phi' (obtained with the constant):
	  	E = {x| ||L^T.(x-phi)|| <= 1},
  	with x a point in the composition space and L^T the transpose of an upper
  	triangular matrix describing the EOA (see below: "Computation of L" ).
  	
  2)To RETRIEVE the mapping from the chemPoint phi, the query point phiq has to 
    be in the EOA of phi. It follows that, dphi=phiq-phi and to test if phiq
    is in the ellipsoid there are two methods. First, compare r=||dphi|| with 
    rmin and rmax. If r < rmin, phiq is in the EOA. If r > rmax, phiq is out of
    the EOA. This operations is O(spaceSize) and is performed first.
    If rmin < r < rmax, then the second method is used:
    	||L^T.dphi|| <= 1 to be in the EOA.
    
    If phiq is in the EOA, Rphiq is obtained by linear interpolation:
    	Rphiq= Rphi + A.dphi.	
  
  3)If phiq is not in the EOA, then the mapping is computed. But as the EOA
    is a conservative approximation of the region of accuracy surrounding the
    point phi, we could expand it by comparing the computed results with the 
    one obtained by linear interpolation. The error epsGrow is calculated:
    	epsGrow = ||B.(dR - dRl)||,
    with dR = Rphiq - Rphi, dRl = A.dphi and B the diagonal scale factor
    matrix.
    If epsGrow <= epsTol, the EOA is too conservative and a GROW is perforned,
    otherwise, the newly computed mapping is associated to the initial composition
    and added to the tree.
  
  4)To GROW the EOA, we expand it to include the previous EOA and the query
  	point phiq. The rank-one matrix method is used. The EOA is transformed
  	to a hypersphere centered at the origin. Then it is expanded to include
  	the transformed point phiq' on its boundary. Then the inverse transformation
  	give the modified matrix L' (see below: "Grow the EOA").
    
    	
  Computation of L :
  	In [1], the EOA of the constant approximation is given by 
  		E = {x| ||B.A/epsTol.(x-phi)|| <= 1},
  	with B a scale factor diagonal matrix, A the mapping gradient matrix and
  	epsTol the absolute tolerance. If we take the QR decomposition of 
  	(B.A)/epsTol= Q.R, with Q an orthogonal matrix and R an upper triangular 
  	matrix such that the EOA is described by (phiq-phi0)^T.R^T.R.(phiq-phi0) <= 1
  	L^T = R, both Cholesky decomposition of A^T.B^T.B.A/epsTol^2
  	This representation of the ellipsoid is used in [2] and in order to avoid 
  	large value of semi-axe length in certain direction, a Singular Value
  	Decomposition (SVD) is performed on the L matrix:
  		L = UDV^T,
  	with the orthogonal matrix U giving the directions of the principal axes
  	and 1/di the inverse of the element of the diagonal matrix D giving the
  	length of the principal semi-axes. To avoid very large value of those length,
	di' = max(di, 1/(alphaEOA*sqrt(epsTol))), with alphaEOA = 0.1 (see [2]) or 
	di' = max(di, 1/2), see [1]. The latter will be used in this implementation.
	And L' = UD'V^T, with D' the diagonal matrix with the modified di'. 
  
  Grow the EOA :
  	More details about the minimum-volume ellipsoid covering an ellispoid E and
  	a point p are found in [3]. Here is the main steps to obtain the modified 
  	matrix L' describind the new ellipsoid.
  		1) calculate the point p' in the transformed space :
  			p' = L^T.(p-phi)
  		2) compute the rank-one decomposition:
  			G = I + gamma.p'.p'^T,
  		   with gamma = (1/|p'|-1)*1/|p'|^2	
  		3) compute L':
  			L' = L.G.
  			
    [1] Stephen B. Pope, "Computationally efficient implementation of
    	combustion chemistry using in situ adaptative tabulation", 
    	Combust. Theory Modelling, 1, 1997
    [2] Liuyan Lu, Stephen B. Pope, "An improved algorithm for in situ
    	adaptative tabulation", JCP, 228, 2009
	[3] Stephen B. Pope, "Algorithms for ellipsoids", FDA 08-01,
		Cornell University, 2008

\*---------------------------------------------------------------------------*/

#ifndef chemPointISAT_H
#define chemPointISAT_H

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
#include "chemPointBase.H"
#include "IOstream.H"
#include "dictionary.H"
#include "Switch.H"
#include "scalarField.H"
#include "OFstream.H"


namespace Foam
{
template<class CompType, class ThermoType>
class binaryNode;
template<class CompType, class ThermoType>
class TDACChemistryModel;

template<class CompType, class ThermoType>
class chemPointISAT
:
    public chemPointBase
{


private:
	
    //Private data
    
    //- Pointer to the chemistryModel object
    TDACChemistryModel<CompType, ThermoType>* chemistry_;
    
    //- Vector storing the composition, temperature and pressure
    scalarField phi_;
    
    //- Vector storing the mapping of the composition phi
    scalarField Rphi_;
    
    //- LT the transpose of the L matrix describing the Ellipsoid Of Accuracy
    //use List of Lists to be able to change size if DAC is used
    List<List<scalar> > LT_;        
    
    //- QT the matrix obtained during QR decomposition (used to update LT after a grow)
    List<List<scalar> > QT_;        
    
    //- A the mapping gradient matrix
    List<List<scalar> >A_;
    
    /*
    //- The minimum length of the principal semi-axes
    scalar rmin_;
    
    //- The maximum length of the principal semi-axes
    scalar rmax_;
    */
    
    //- Vector storing the scale factor
    scalarField scaleFactor_;        
    
    //- Reference to the node in the binary tree holding this chemPoint
    binaryNode<CompType, ThermoType>* node_;
    
    //- The size of the composition space (size of the vector phi)
    label spaceSize_;
    
    //- Number of times the element has been used
    label nUsed_;
    
    //- Number of times the element has been grown
    label nGrown_;
    
    //- Tolerance for the Ellipsoid of accuracy
    static scalar epsTol_;
    
    //- Variables related to DAC
    //Switch to know if DAC is active
    Switch DAC_;
    
    //Number of species obtained by DAC_ when constructing the chemPoint
    label NsDAC_;
    
    //Vectors that store the index conversion between the simplified
    //and the complete chemical mechanism
    List<label> completeToSimplifiedIndex_;
    List<label> simplifiedToCompleteIndex_;
    
/*    fileName failedSpeciesFile_;
    ofstream failedSpecies_;
    
    const Time* refTime_;*/
    
    label inertSpecie_;
    
    scalar timeTag_;
    scalar lastTimeUsed_;
    
    scalar lastError_;
    bool toRemove_;
    //- Use logarithm of temperature                
    //Switch logT_;
    
    //- Use tauStar for EDC
    //Switch tauStar_;
    
    
    /*---------------------------------------------------------------------------*\
     QR decomposition of the matrix A (implementation based on the one in
     DenseMatrixTools but here A is not modified in order to keep it and
     store it along with the qr decomposition which describe the 
     initial ellipsoid of accuracy)
     Input : nCols cols number
     A the matrix to decompose A = Q.R
     R empty matrix in which the upper triangular matrix is stored
     Output: void
     Note : in this implementation we are only interested in the R matrix
     \*---------------------------------------------------------------------------*/
    void qrDecompose
    (
     const label nCols,
     List<List<scalar> >& A
     );
    
    void qrUpdate
    (
     const label n,
     const scalarField &u,
     const scalarField &v
     );
    
    void rotate
    (const label i, const scalar a, const scalar b,
     label n
     );
    /*---------------------------------------------------------------------------*\
     Singular Value Decomposition (SVD) for a square matrix
     needed to compute the the length of the hyperellipsoid semi-axes
     SVD decompose a matrix A into:
     A = U * D * V^T , 
     with the singular value in the diagonal matrix D and U and V orthogonal 
     Input :  A (scalarMatrix) the square matrix to apply the decomposition on
     m (label) the number of line of the matrix A
     n (label) the size of the matrix A
     Output:  U (scalarMatrix) replace A
     V (scalarMatrix) not the transpose V^T
     d (scalarField) the diagonal element of matrix D
     The algorithm is based on "Numerical recipes in C", second edition (1992)	
     chapter 2, pp 67-70	  
     \*---------------------------------------------------------------------------*/
    void svd(List<List<scalar> >& A, label m, label n, scalarField& d, List<List<scalar> >& V);
    scalar pythag(scalar a, scalar b); //function used in svd function
    

                               
public:
    
    //- Constructors
    //- Construct from components
    chemPointISAT
    (
     TDACChemistryModel<CompType, ThermoType>& chemistry,
     const scalarField& phi,
     const scalarField& Rphi,
     const List<List<scalar> >& A,
     const scalarField& scaleFactor,
     const scalar& epsTol,
     const label& spaceSize,
     binaryNode<CompType, ThermoType>* node = NULL
     );
    
    //- Construct from components and reference to a binary node
    /*chemPoint
     (
     const chemistryModel& chemistry,
     const scalarField& phi,
     const scalarField& Rphi,
     const Matrix<scalar>& A,
     const scalarField& scaleFactor,
     const scalar& epsTol,
     const label& spaceSize,
     binaryNode* node
     );
     */
    //- Construct from another chemPoint and reference to a binary node
    chemPointISAT
    (
     const chemPointISAT<CompType, ThermoType>& p,
     binaryNode<CompType, ThermoType>* node 
     );
    
    
    //- Construct from another chemPoint
    chemPointISAT
    (
     chemPointISAT<CompType, ThermoType>& p
     );
    
    
    //- Access
    
    inline label nUsed()
    {
        return nUsed_;
    }
    
    inline label nGrown()
    {
        return nGrown_;
    }
    
    inline const label& nUsed() const
    {
        return nUsed_;
    }
    
    inline label& spaceSize()
    {
        return spaceSize_;
    }
    
    inline const scalarField& phi() const
    {
        return phi_;
    }
    
    inline const scalarField& Rphi() const
    {
        return Rphi_;
    }
    
    inline const scalarField& scaleFactor()
    {
        return scaleFactor_;
    }
    
    inline const scalar& epsTol()
    {
        return epsTol_;
    }
    
    static void changeEpsTol(scalar newTol)
    {
        epsTol_ = newTol;
    }
       
    inline binaryNode<CompType, ThermoType>*& node()
    {
        return node_;
    }
    
    inline const List<List<scalar> >& A() const
    {
        return A_;
    }
    
    inline List<List<scalar> >& A() 
    {
        return A_;
    } 
    inline const List<List<scalar> >& LT() const
    {
        return LT_;
    }
    
    inline List<List<scalar> >& LT()
    {
        return LT_;
    }
    
    inline List<List<scalar> >& QT()
    {
        return QT_;
    }
    
    //Switch to know if DAC is active
    inline Switch DAC()
    {
        return DAC_;
    }
    
    //Number of species obtained by DAC_ when constructing the chemPoint
    inline label NsDAC()
    {
        return NsDAC_;
    }
    
    //Vectors that store the index conversion between the simplified
    //and the complete chemical mechanism
    inline List<label>& completeToSimplifiedIndex()
    {
        return completeToSimplifiedIndex_;
    }
    
    inline List<label>& simplifiedToCompleteIndex()
    {
        return simplifiedToCompleteIndex_;
    }
    inline label completeToSimplifiedIndex(label i)
    {
        return completeToSimplifiedIndex_[i];
    }
    
    inline label simplifiedToCompleteIndex(label i)
    {
        return simplifiedToCompleteIndex_[i];
    }
    
    inline label inertSpecie()
    {
        return inertSpecie_;
    }
    
    inline const scalar& timeTag()
    {
        return timeTag_;
    }
    
    inline scalar& lastTimeUsed()
    {
        return lastTimeUsed_;
    }
    
    inline scalar& lastError()
    {
        return lastError_;
    }
    
    inline bool& toRemove()
    {
        return toRemove_;
    }
    /*
    inline fileName failedSpeciesFile()
    {
        return failedSpeciesFile_;
    }
    */
    // is the point in the ellipsoid of accuracy?
    bool inEOA(const scalarField& phiq);
    inline bool checkError(const scalarField& phiq)
    {
        return inEOA(phiq);
    }
    
    // grow the ellipsoid of accuracy?
    bool grow(const scalarField& phiq);
    
    // check if the new solution is in the ellipsoid of accuracy?
    bool checkSolution(const scalarField& phiq, const scalarField& Rphiq);
    
    // set free the point from its node, used for replacing purposes in the binary tree
    void setFree();
    
    // clear all the stored data
    void clearData();
    
};
    
}
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#ifdef NoRepository
#   include "chemPointISAT.C"
#endif
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //


#endif

