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
    Binary Search Tree based on the Red-Black tree model described in 
    "Introduction to Algorithms"

\*---------------------------------------------------------------------------*/

#include "binaryTree.H"
#include "binaryNode.H"
#include "demandDrivenData.H"
#include "clockTime.H"
#include "Random.H"


namespace Foam
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //
template<class CompType, class ThermoType>
binaryTree<CompType, ThermoType>::binaryTree
(
 TDACChemistryModel<CompType, ThermoType>& chemistry,
 dictionary coeffsDict
) 
:
    chemistry_(chemistry),
    root_(NULL),
    maxElements_(readLabel(coeffsDict.lookup("maxElements"))),
    size_(0),
    n2ndSearch_(0),
    max2ndSearch_(coeffsDict.lookupOrDefault("max2ndSearch",0)),
    minBalanceThreshold_(coeffsDict.lookupOrDefault("minBalanceThreshold",0.1*maxElements_)),
    maxNbBalanceTest_(coeffsDict.lookupOrDefault("maxNbBalanceTest",0.01*chemistry_.nSpecie())),
    balanceProp_(coeffsDict.lookupOrDefault("balanceProp",0.35))
{}

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //


// * * * * * * * Insert New Leaf * * * * * * //
/*---------------------------------------------------------------------------*\
	Insert a new leaf starting from the parent node of phi0
	Input : phi0 the leaf to replace by a node
			phiq the new composition to store
			Rphiq the mapping of the new composition point
			A the mapping gradient matrix
			B the matrix used to initialize the EOA
			nCols the size of the matrix
	Output: void
	Description : 
			1) Create a new leaf with the data to initialize the EOA and to
				retrieve the mapping by linear interpolation (the EOA is 
				initialize in the chemPoint constructor)
			2) Get the parent node of phi0 and connect a new node in place of the 
				leaf of phi0. This new node is constructed with phi0 on the left
				and phiq on the right (the hyperplane is computed inside the 
				binaryNode constructor)
\*---------------------------------------------------------------------------*/
template<class CompType, class ThermoType>
void binaryTree<CompType, ThermoType>::insertNewLeaf
(
 const scalarField& phiq,
 const scalarField& Rphiq, 
 const List<List<scalar> >& A, 
 const scalarField& scaleFactor, 
 const scalar& epsTol,
 const label nCols,
 chP*& phi0
 )
{

    if(size_ == 0) //no points are stored
    {
        //create an empty binary node and root points to it
        root_ = new bn();
        //create the new chemPoint which holds the composition point
        //phiq and the data to initialize the EOA
        chP* newChemPoint =
            new chP(chemistry_,phiq, Rphiq, A, scaleFactor, epsTol, nCols,root_);
        root_->elementLeft()=newChemPoint;
    }
    else //at least one point stored
    {
        //no reference chemPoint, a BT search is required
        if(phi0 == NULL) 
        {
            chemPointBase* phi0Base;
            binaryTreeSearch(phiq, root_,phi0Base);
            phi0 = dynamic_cast<chP*>(phi0Base);
        }
        //access to the parent node of the chemPoint
        bn* parentNode = phi0->node();
        
        //create the new chemPoint which holds the composition point
        //phiq and the data to initialize the EOA
        chP* newChemPoint =
            new chP(chemistry_,phiq, Rphiq, A, scaleFactor, epsTol, nCols);
        //insert new node on the parent node in the position of the
        //previously stored leaf (phi0)
        //the new node contains phi0 on the left and phiq on the right
        //the hyper plane is computed in the binaryNode constructor
        bn* newNode;
        if(size_>1)
        {
            newNode = new bn(phi0, newChemPoint, parentNode);
            insertNode(phi0, newNode);//make the parent of phi0 point to the newly created node
        }
        else //size_ == 1 (because not equal to 0)
        {
            deleteDemandDrivenData(root_);//when size is 1, the binaryNode is an empty contained without hyperplane
            newNode = new bn(phi0, newChemPoint, NULL);
            root_ = newNode;
        }   
        
        phi0->node()=newNode;
        newChemPoint->node()=newNode;
    }
    size_++;

}


//insert new node at the position of phi0
//phi0 should be already attached to another node or the pointer to it will be lost
template<class CompType, class ThermoType>
void binaryTree<CompType, ThermoType>::insertNode
(
	chP*& phi0,
	bn*& newNode
)
{
    if (phi0==phi0->node()->elementRight())//phi0 is on the right
    {
	phi0->node()->elementRight() = NULL;    
        phi0->node()->right() = newNode;
    }
    else
    {
        phi0->node()->elementLeft() = NULL;    
        phi0->node()->left() = newNode;
        
    }
}
        
// * * * * * * end of insertNewLeaf functions * * * * * * //


//Search the binaryTree until the nearest leaf of a specified
//leaf is found. 
template<class CompType, class ThermoType>
void binaryTree<CompType, ThermoType>::binaryTreeSearch(const scalarField& phiq, bn* node, chemPointBase*& nearest)
{
    if (size_ > 1)
    {
        scalar vPhi=0.0;
        const scalarField& v = node->v();
        const scalar& a = node->a();
        //compute v*phi
        for (label i=0; i<phiq.size(); i++) vPhi += phiq[i]*v[i];
        
        
        if(vPhi > a) //on right side (side of the newly added point)
        {
            if (node->right()!=NULL)
            {
                binaryTreeSearch(phiq, node->right(), nearest);
            }
            else //the terminal node is reached, return element on right
            {
                nearest = node->elementRight();
            }
        }
        else //on left side (side of the previously stored point)
        {
            if (node->left()!=NULL)
            {
                binaryTreeSearch(phiq, node->left(), nearest);
            }
            else //the terminal node is reached, return element on right
            {
                nearest = node->elementLeft();
            }
        }
    }
    // only one point stored (left element of the root)
    else if (size_ == 1)
    {
        nearest = root_->elementLeft();
    }
    else //no point stored
    {
        nearest = NULL;
    }
}//end binaryTreeSearch


//Compute the depth of the binary tree (starting from the root as default)
template<class CompType, class ThermoType>
label binaryTree<CompType, ThermoType>::depth(bn* subTreeRoot)
{
    if(subTreeRoot == NULL)
    {
        return 0;
    }
    else 
    {
        return 1+max(depth(subTreeRoot->left()),depth(subTreeRoot->right()));
    }
}


//- Delete a leaf from the binary tree and reshape the binary tree for the
//  following binary tree search and to maintain the Red-Black Tree properties
//  Note: compared to the pseudocode provided in "Introduction to Algortihms", 
//        we do not use the third option (else statement of -DELETE, line 9)
//        because we always have a node with at least one NULL child
template<class CompType, class ThermoType>
void binaryTree<CompType, ThermoType>::deleteLeaf(chP*& phi0)
{

    if(size_ == 1) //only one point is stored
    {
        deleteDemandDrivenData(phi0);
        deleteDemandDrivenData(root_);
    }
    else if (size_ > 1)
    {
        bn* z = phi0->node();
        bn* x;
        chP* siblingPhi0 = chemPSibling(phi0);
              
        if (siblingPhi0 != NULL)//the sibling of phi0 is a chemPoint
        {
            if (z->parent() == NULL) //z was root (only two chemPoints in the tree)
            {
                root_ = new bn();
                root_->elementLeft()=siblingPhi0;
                siblingPhi0->node()=root_;
                
            }
            else if (z==z->parent()->left())
            {
                z->parent()->elementLeft() = siblingPhi0;
                z->parent()->left() = NULL;
                siblingPhi0->node() = z->parent();
            }
            else 
            {
                z->parent()->elementRight() = siblingPhi0;
                z->parent()->right() = NULL;
                siblingPhi0->node() = z->parent();
            }
        }
        else
        {
            x = nodeSibling(phi0);
            transplant(z,x);
        }
        
        deleteDemandDrivenData(phi0);
        deleteDemandDrivenData(z);
    }
    size_--;
}//end of deleteLeaf


template<class CompType, class ThermoType>
void binaryTree<CompType, ThermoType>::transplant(bn* u, bn* v)
{
    if (u->parent() == NULL)
    {
        root_ = v;
    }
    else if (u == u->parent()->left())
    {
        u->parent()->left() = v;
    }
    else 
    {
        u->parent()->right()=v;
    }
    
    if (v != NULL)
    {
        v->parent() = u->parent();
    }

}

//Remove every entries of the tree and delete the associated objects
template<class CompType, class ThermoType>
void binaryTree<CompType, ThermoType>::clear()
{

    //recursively delete the element in the subTree
    deleteSubTree();
    
    //reset root node (should already be NULL)
    root_=NULL;
    
    //reset size_
    size_=0;
    
}//end cleanAll

template<class CompType, class ThermoType>
void binaryTree<CompType, ThermoType>::deleteSubTree(bn* subTreeRoot)
{
    //no need to check for nullness it delete everything in the tree even NULL pointer (no effect)
    if(subTreeRoot != NULL)
    {
        deleteDemandDrivenData(subTreeRoot->elementLeft());
        deleteDemandDrivenData(subTreeRoot->elementRight());
        deleteSubTree(subTreeRoot->left());
        deleteSubTree(subTreeRoot->right());
        deleteDemandDrivenData(subTreeRoot);
    }
}

template<class CompType, class ThermoType>
void binaryTree<CompType, ThermoType>::deleteAllNode(bn* subTreeRoot)
{
    if(subTreeRoot != NULL)
    {
        deleteAllNode(subTreeRoot->left());
        deleteAllNode(subTreeRoot->right());
        deleteDemandDrivenData(subTreeRoot);
    }
}


//Check if the tree has reached the maximum number of elements
template<class CompType, class ThermoType>
bool binaryTree<CompType, ThermoType>::isFull()
{
    return size_ >= maxElements_;   
}

//Perform a secondary binary tree search starting from a failed chemPoint x
//If another candidate is found return true and x points to the chemPoint
//The nUsed features is handled at ISAT level
template<class CompType, class ThermoType>
bool binaryTree<CompType, ThermoType>::secondaryBTSearch
(
    const scalarField& phiq, 
    chP*& x
)
{
    //initialize n2ndSearch_
    n2ndSearch_ = 0;
    if((n2ndSearch_ < max2ndSearch_) && (size_ > 1))
    {
        chP* xS = chemPSibling(x);
        if(xS != NULL)
        {
            n2ndSearch_++;
            if(xS->inEOA(phiq))
            {
                x=xS;
                return true;
            }
        }
        else if (inSubTree(phiq,nodeSibling(x),x))
        {
            return true;
        }
        bn* y = x->node();
        while((y->parent()!= NULL) && (n2ndSearch_ < max2ndSearch_))
        {
            xS = chemPSibling(y);
            if(xS != NULL)
            {
                n2ndSearch_++;
                if(xS->inEOA(phiq))
                {
                    x=xS;
                    return true;
                }
            }
            else if(inSubTree(phiq,nodeSibling(y),x))
            {
                return true;
            }
            y=y->parent();
        }
        //if we reach this point it is either because 
        //we did not find another covering EOA or 
        //we reach the maximm number of secondary search
        return false;
    }
    else
    {
        return false;
    }

}

//Perform a search in the subtree starting from the subtree node y
//This search continue to use the hyperplan to walk the tree
//If covering EOA is found return true and x points to the chemPoint
//CAUTION : x pointer should not be used if inSubTree returns false
template<class CompType, class ThermoType>
bool binaryTree<CompType, ThermoType>::inSubTree
(
    const scalarField& phiq, 
    bn* y,
    chP*& x
)
{
    if((n2ndSearch_ < max2ndSearch_) && (y!=NULL))
    {
        scalar vPhi=0.0;
        const scalarField& v = y->v();
        const scalar& a = y->a();
        //compute v*phi
        for (label i=0; i<phiq.size(); i++) vPhi += phiq[i]*v[i];
        
        if(vPhi<=a)//on the left side of the node
        {
            if(y->left() == NULL)//left is a chemPoint
            {
                n2ndSearch_++;
                x=y->elementLeft();
                if(x->inEOA(phiq))
                {
                    return true;
                }
            }
            else//the left side is a node
            {
                if(inSubTree(phiq,y->left(),x))
                {
                    return true;
                }
            }    
            
            if((n2ndSearch_ < max2ndSearch_) && y->right() == NULL)
            {
                n2ndSearch_++;
                x=y->elementRight();
                return x->inEOA(phiq);
            }
            else//test for n2ndSearch is done in the call of inSubTree
            {
                return inSubTree(phiq,y->right(),x);
            }
            
                            
        }//end of "on left side"
        else //on right side (symetric of above)
        {
            if(y->right() == NULL)
            {
                n2ndSearch_++;
                x=y->elementRight();
                if(x->inEOA(phiq))
                {
                    return true;
                }
            }
            else//the right side is a node
            {
                if(inSubTree(phiq,y->right(),x))
                {
                    return true;
                }
            }
                        
            //if we reach this point, the retrieve has 
            //failed on the right side, explore the left side
            if((n2ndSearch_ < max2ndSearch_) && y->left() == NULL)
            {
                n2ndSearch_++;
                x=y->elementLeft();
                return x->inEOA(phiq);
            }
            else 
            {
                return inSubTree(phiq,y->left(),x);
            }
        }
    }//end if((n2ndSearch_ < max2ndSearch_) && (y!=NULL))
    else 
    {
        return false;
    }
}


template<class CompType, class ThermoType>
chemPointISAT<CompType, ThermoType>* binaryTree<CompType, ThermoType>::chemPSibling
(
    bn* y
)
{
    if(y->parent()!=NULL)
    {
        if(y==y->parent()->left())//y is on the left, return right side
        {
            return y->parent()->elementRight();
        }
        else //y is on the right, return left side
        {
            return y->parent()->elementLeft();
        }
    }
    else 
    {
        return NULL;
    }

}
template<class CompType, class ThermoType>
chemPointISAT<CompType, ThermoType>* binaryTree<CompType, ThermoType>::chemPSibling
(
    chP* x
)
{
    if(size_>1)
    {
        bn* y = x->node();
        if(x==y->elementLeft())//x is on the left, return right side
        {
            return y->elementRight();
        }
        else//x is on the right, return left side
        {
            return y->elementLeft();
        }
    }
    else 
    {
        return NULL;
    }

}
template<class CompType, class ThermoType>
binaryNode<CompType, ThermoType>* binaryTree<CompType, ThermoType>::nodeSibling
(
    bn* y
)
{
    if(y->parent()!=NULL)
    {
        if(y==y->parent()->left())//y is on the left, return right side
        {
            return y->parent()->right();
        }
        else //y is on the right, return left side
        {
            return y->parent()->left();
        }
    }
    else 
    {
        return NULL;
    }
}
template<class CompType, class ThermoType>
binaryNode<CompType, ThermoType>* binaryTree<CompType, ThermoType>::nodeSibling
(
    chP* x
)
{
    if(size_>1)
    {
        bn* y = x->node();
        if(x==y->elementLeft())//x is on the left, return right side
        {
            return y->right();
        }
        else//x is on the right, return left side
        {
            return y->left();
        }
    }
    else 
    {
        return NULL;
    }
}

//- Cheap balance function:
//  This function just roughly separate the space in two parts 
//  with a hyperplane perpendicular to the variance vector.
//  Then, it repopulate the tree with this hyperplane stored at the root
//  and by inserting the chemPoint randomly which should give a depth O(log_2(n))
template<class CompType, class ThermoType>
bool binaryTree<CompType, ThermoType>::balance()
{
    if(size_ > minBalanceThreshold_)
    {
        scalarField mean(chemistry_.nEqns(),0.0);//use the size of the space
        SortableList<scalar> variance(chemistry_.nEqns(),0.0);//use the size of the space
        
        //1) walk through the entire tree by starting with the tree most left chemPoint
        chP* x=treeMin();
        List<chP*> chemPoints(size_);
        label chPi=0;
        while(x!=NULL)
        {
            const scalarField& phij = x->phi(); 
            //2) compute the mean composition 
            mean += phij;
            chemPoints[chPi++]=x;
            x=treeSuccessor(x);
        }    
        mean /= size_;

        //3) compute the variance for each space direction
        forAll(chemPoints,j)
        {
            const scalarField& phij = chemPoints[j]->phi();
            forAll(variance,vi)
            {
                variance[vi] += sqr(phij[vi]-mean[vi]);
            }    
        }
        
        //4) analyze which direction of the cutting plane better separate the points (maximal variance) 
        variance.sort();
        label maxDir(-1);
        //maxDir indicates the direction of maximum variance
        //the vector v[maxDir]=1; v[i!=maxDir]=0 indicates the perpendicular direction
        //and can be used to define the hyperplane (like in DOLFA)
        //instead we create the new root node by taking the two extreme points in this direction 
        //if these extreme points were not deleted in the cleaning that come before the equilibrate
        // they are still important and the tree should therefore take them into account
        label nbLeft(0);
        label nbTests(0);
        scalar bestBalance(size_);
        while
        (
            ((nbLeft<balanceProp_*size_) || (nbLeft>((1-balanceProp_)*size_)))
            &&
            ((nbTests<maxNbBalanceTest_) && (nbTests<variance.size()-1))
        )
        {
            nbLeft=0;
            nbTests++;

            //variance.indices refers to the directions in the composition spaces
            //variance.indices[variance.size()-(nbTests)] refers to the direction
            //of the nbTests highest value of variance
            label curDir = variance.indices()[variance.size()-(nbTests)];
            forAll(chemPoints,j)
            {
                scalar phiMaxDir = chemPoints[j]->phi()[curDir];
                if(phiMaxDir < mean[curDir])
                {
                    nbLeft++; 
                }
              
            }
            //with bestBalance starting at size
            //the first while loop will go in this if loop
            if(fabs(nbLeft-size_*0.5)<bestBalance)
            {
                bestBalance = fabs(nbLeft-size_*0.5);
                maxDir = curDir;
            }
        }

        scalar maxPhi(0);
        scalar minPhi(GREAT);
        label minId(-1);
        label maxId(-1);
        forAll(chemPoints,j)
        {
            scalar phiMaxDir = chemPoints[j]->phi()[maxDir];
            if(phiMaxDir>maxPhi)
            {
                maxId = j;
                maxPhi=phiMaxDir;
            }
            if(phiMaxDir<minPhi)
            {
                minId = j;
                minPhi = phiMaxDir;
            }
        }
        chP* minRef = chemPoints[minId];
        chP* maxRef = chemPoints[maxId];
        
        //delete reference to all node since the tree is reshaped
        deleteAllNode();
        root_=NULL;

        
        //add the node for minRef and maxRef
        
        bn* newNode = new bn(minRef, maxRef, NULL);
        root_ = newNode;
        minRef->node() = newNode;
        maxRef->node() = newNode;
        
        //construct the new tree by adding the chemPoints 
        //without using minRef and maxRef => test for maxId and minId
        //random access to the chemPoint to try to maintain depth O(logn)
        labelList chPIndex = identity(size_);//cellIndexTmp[i]=i
        Random randGenerator(unsigned(time(NULL)));
        label j;
        for (label i=0; i<size_; i++)
        {
            j=randGenerator.integer(i,size_-1);
            label tmp = chPIndex[i];
            chPIndex[i] = chPIndex[j];
            chPIndex[j] = tmp;
        }  

        forAll(chemPoints,cpi)
        {
            if((chPIndex[cpi]!=minId) && (chPIndex[cpi]!=maxId))
            {
                //search tree for position
                chemPointBase* phi0Base;
                binaryTreeSearch(chemPoints[chPIndex[cpi]]->phi(),root_,phi0Base);
                chP* phi0 = dynamic_cast<chP*>(phi0Base);
                //add the chemPoint
                bn* nodeToAdd = new bn(phi0,chemPoints[chPIndex[cpi]], phi0->node());
                insertNode(phi0, nodeToAdd);//make the parent of phi0 point to the newly created node
                phi0->node()=nodeToAdd;
                chemPoints[chPIndex[cpi]]->node()=nodeToAdd;
            }
        }
        return true;
    }//end if
    else
    { 
        return false;
    }
}

template<class CompType, class ThermoType>
chemPointISAT<CompType, ThermoType>* binaryTree<CompType, ThermoType>::treeMin(bn* subTreeRoot)
{
    if(subTreeRoot!=NULL)
    {
        while(subTreeRoot->left() != NULL)
        {
            subTreeRoot = subTreeRoot->left();
        }
        return subTreeRoot->elementLeft();
    }
    else 
    {
        return NULL;
    }

}

template<class CompType, class ThermoType>
chemPointISAT<CompType, ThermoType>* binaryTree<CompType, ThermoType>::treeSuccessor(chP* x)
{
    if(size_>1)
    {
        if(x==x->node()->elementLeft())
        {
            bn* parentNode = x->node();
            if(parentNode->right()==NULL)
            {
                return parentNode->elementRight();
            }
            else 
            {
                return treeMin(parentNode->right());
            }
        }
        else
        {
            bn* y = x->node();
            while((y->parent() !=NULL))
            {
                if(y==y->parent()->left())
                {
                    if(y->parent()->right()==NULL)
                    {
                        return y->parent()->elementRight();
                    }
                    else 
                    {
                        return treeMin(y->parent()->right());
                    }
                }
                y=y->parent();
            }
            //when we reach this point, y points to the root and
            //never entered in the if loop (coming from the right)
            //so we are at the tree maximum and there is no successor
            return NULL; 
        }
    }
    else 
    {
        return NULL;
    }
}


} // End namespace Foam


